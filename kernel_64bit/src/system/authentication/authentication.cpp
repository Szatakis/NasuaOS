#include "authentication.hpp"

#include <cstddef>
#include <cstdint>

#include "shell/shell.hpp"
#include "drivers/disk/driver.hpp"
#include "drivers/gpu/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "system/applications/napp/napp.hpp"
#include "system/filesystem/clawfs/clawfs.hpp"
#include "system/sysfunc/rand/rand.hpp"
#include "system/vars/info_vars/info_vars.hpp"
#include "libs/libc/libc.hpp"

#define strlen bearssl_compat_strlen
#include "bearssl.h"
#undef strlen

namespace
{
    constexpr uint32_t MAX_USERS = 16;
    constexpr uint32_t USERNAME_CAPACITY = 16;
    constexpr uint32_t PASSWORD_CAPACITY = 64;
    constexpr uint32_t SALT_SIZE = 16;
    constexpr uint32_t HASH_SIZE = 32;
    constexpr uint32_t PASSWORD_HASH_ITERATIONS = 100000;
    constexpr uint32_t DATABASE_CAPACITY = 4096;
    constexpr char DATABASE_PATH[] = "/etc/accounts.db";
    constexpr char PERMISSIONS_PATH[] = "/etc/permissions.db";
    constexpr uint32_t MAX_PATH_PERMISSIONS = 32;
    constexpr uint32_t PATH_CAPACITY = 128;

    struct account_t
    {
        char name[USERNAME_CAPACITY];
        uint8_t salt[SALT_SIZE];
        uint8_t password_hash[HASH_SIZE];
        bool administrator;
    };

    struct path_permission_t
    {
        char path[PATH_CAPACITY];
        char owner[USERNAME_CAPACITY];
        uint16_t mode;
    };

    enum screen_mode_t
    {
        SCREEN_HIDDEN,
        SCREEN_LOGIN_USERNAME,
        SCREEN_LOGIN_PASSWORD,
        SCREEN_NEW_PASSWORD,
        SCREEN_CONFIRM_PASSWORD,
        SCREEN_DATABASE_RECOVERY
    };

    enum password_action_t
    {
        PASSWORD_ACTION_NONE,
        PASSWORD_ACTION_ADD_USER,
        PASSWORD_ACTION_CHANGE_PASSWORD
    };

    account_t accounts[MAX_USERS];
    uint32_t account_count = 0;
    bool database_valid = false;
    bool storage_available = false;
    bool login_enabled = false;
    char automatic_login[USERNAME_CAPACITY] = "user";
    char input[USERNAME_CAPACITY] = "user";
    char password_input[PASSWORD_CAPACITY] = {};
    char pending_password[PASSWORD_CAPACITY] = {};
    char password_target[USERNAME_CAPACITY] = {};
    char status_message[96] = {};
    screen_mode_t screen_mode = SCREEN_HIDDEN;
    password_action_t password_action = PASSWORD_ACTION_NONE;
    bool switching_user = false;
    bool current_user_valid = false;
    bool recovery_armed = false;
    int active_account_index = -1;
    path_permission_t path_permissions[MAX_PATH_PERMISSIONS];
    uint32_t path_permission_count = 0;
    bool internal_auth_storage_access = false;

    void clear_secret(char* secret, size_t length)
    {
        volatile char* bytes = secret;
        while (length-- > 0)
        {
            *bytes++ = '\0';
        }
    }

    void set_status(const char* message)
    {
        size_t i = 0;
        while (message[i] != '\0' && i < sizeof(status_message) - 1)
        {
            status_message[i] = message[i];
            ++i;
        }
        status_message[i] = '\0';
    }

    bool valid_username(const char* name)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return false;
        }

        size_t length = 0;
        while (name[length] != '\0')
        {
            const char c = name[length];
            if (length >= USERNAME_CAPACITY - 1 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            {
                return false;
            }
            ++length;
        }
        return true;
    }

    char* find_character(char* text, char character)
    {
        while (*text != '\0')
        {
            if (*text == character)
            {
                return text;
            }
            ++text;
        }
        return nullptr;
    }

    bool valid_password(const char* password)
    {
        if (password == nullptr)
        {
            return false;
        }
        const size_t length = strlen(password);
        return length > 0 && length < PASSWORD_CAPACITY;
    }

    int find_account(const char* name)
    {
        for (uint32_t i = 0; i < account_count; ++i)
        {
            if (strcmp(accounts[i].name, name) == 0)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool path_is_within(const char* path, const char* parent)
    {
        const size_t parent_length = strlen(parent);
        return strncmp(path, parent, parent_length) == 0 &&
            (path[parent_length] == '\0' || path[parent_length] == '/');
    }

    bool valid_permission_path(const char* path)
    {
        if (path == nullptr || path[0] != '/' ||
            strlen(path) >= PATH_CAPACITY)
        {
            return false;
        }
        if (path[1] == '\0')
        {
            return true;
        }

        size_t component_start = 1;
        for (size_t i = 1; path[i] != '\0'; ++i)
        {
            if (path[i] == '|' || path[i] == '\n' || path[i] == '\r')
            {
                return false;
            }
            if (path[i] == '/')
            {
                const size_t component_length = i - component_start;
                if (component_length == 0 ||
                    (component_length == 1 && path[component_start] == '.') ||
                    (component_length == 2 && path[component_start] == '.' && path[component_start + 1] == '.'))
                {
                    return false;
                }
                component_start = i + 1;
            }
        }

        const size_t final_length = strlen(path) - component_start;
        return final_length != 0 &&
            !(final_length == 1 && path[component_start] == '.') &&
            !(final_length == 2 && path[component_start] == '.' && path[component_start + 1] == '.');
    }

    bool resolve_permission_path(const char* input_path, char output[PATH_CAPACITY])
    {
        if (input_path == nullptr || input_path[0] == '\0')
        {
            return false;
        }

        size_t length = 0;
        if (input_path[0] == '/')
        {
            while (input_path[length] != '\0' && length < PATH_CAPACITY)
            {
                output[length] = input_path[length];
                ++length;
            }
        }
        else
        {
            const size_t cwd_length = strlen(current_path);
            if (cwd_length + 1 >= PATH_CAPACITY)
            {
                return false;
            }
            for (size_t i = 0; i < cwd_length; ++i)
            {
                output[length++] = current_path[i];
            }
            if (length == 0 || output[length - 1] != '/')
            {
                output[length++] = '/';
            }
            size_t source_index = 0;
            while (input_path[source_index] != '\0')
            {
                if (length >= PATH_CAPACITY)
                {
                    return false;
                }
                output[length++] = input_path[source_index];
                ++source_index;
            }
        }

        if (length >= PATH_CAPACITY)
        {
            return false;
        }
        output[length] = '\0';
        while (length > 1 && output[length - 1] == '/')
        {
            output[--length] = '\0';
        }
        return valid_permission_path(output);
    }

    int find_exact_path_permission(const char* path)
    {
        for (uint32_t i = 0; i < path_permission_count; ++i)
        {
            if (strcmp(path_permissions[i].path, path) == 0)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    const char* home_directory_owner(const char* path)
    {
        if (!path_is_within(path, "/home") || strcmp(path, "/home") == 0)
        {
            return nullptr;
        }

        const char* owner = path + 6;
        size_t owner_length = 0;
        while (owner[owner_length] != '\0' && owner[owner_length] != '/')
        {
            ++owner_length;
        }
        if (owner_length == 0 || owner_length >= USERNAME_CAPACITY)
        {
            return nullptr;
        }

        static char owner_name[USERNAME_CAPACITY];
        for (size_t i = 0; i < owner_length; ++i)
        {
            owner_name[i] = owner[i];
        }
        owner_name[owner_length] = '\0';
        return find_account(owner_name) >= 0 ? owner_name : nullptr;
    }

    int find_path_permission(const char* path)
    {
        int best_match = -1;
        size_t best_length = 0;
        for (uint32_t i = 0; i < path_permission_count; ++i)
        {
            const size_t candidate_length = strlen(path_permissions[i].path);
            if (candidate_length > best_length &&
                path_is_within(path, path_permissions[i].path))
            {
                best_match = static_cast<int>(i);
                best_length = candidate_length;
            }
        }
        return best_match;
    }

    bool auth_storage_path_allowed(const char* path, bool write)
    {
        return (strcmp(path, DATABASE_PATH) == 0 ||
                strcmp(path, PERMISSIONS_PATH) == 0 ||
                (write && (strcmp(path, "/etc") == 0 ||
                           strcmp(path, "/") == 0)));
    }

    struct auth_storage_access_guard_t
    {
        bool previous;
        auth_storage_access_guard_t() : previous(internal_auth_storage_access)
        {
            internal_auth_storage_access = true;
        }
        ~auth_storage_access_guard_t()
        {
            internal_auth_storage_access = previous;
        }
    };

    int current_account()
    {
        if (!current_user_valid || active_account_index < 0 || static_cast<uint32_t>(active_account_index) >= account_count)
        {
            return -1;
        }

        return strcmp(user_name, accounts[active_account_index].name) == 0 ? active_account_index : -1;
    }

    bool is_administrator(int index)
    {
        return index >= 0 && static_cast<uint32_t>(index) < account_count && accounts[index].administrator && strcmp(accounts[index].name, "root") == 0;
    }

    void generate_salt(uint8_t salt[SALT_SIZE])
    {
        for (uint32_t i = 0; i < SALT_SIZE; i += 2)
        {
            const uint16_t random = static_cast<uint16_t>(system_rand());
            salt[i] = static_cast<uint8_t>(random);
            salt[i + 1] = static_cast<uint8_t>(random >> 8);
        }
    }

    void derive_password_hash(const char* password, const uint8_t salt[SALT_SIZE], uint8_t output[HASH_SIZE])
    {
        const size_t password_length = strlen(password);
        br_hmac_key_context key_context;
        br_hmac_key_init(&key_context, &br_sha256_vtable, password, password_length);

        uint8_t block[20];
        for (uint32_t i = 0; i < SALT_SIZE; ++i)
        {
            block[i] = salt[i];
        }
        block[16] = 0;
        block[17] = 0;
        block[18] = 0;
        block[19] = 1;

        uint8_t current[HASH_SIZE];
        uint8_t result[HASH_SIZE];
        br_hmac_context hmac;
        br_hmac_init(&hmac, &key_context, 0);
        br_hmac_update(&hmac, block, sizeof(block));
        br_hmac_out(&hmac, current);
        Memory::memcpy(result, current, sizeof(result));

        for (uint32_t iteration = 1; iteration < PASSWORD_HASH_ITERATIONS; ++iteration)
        {
            br_hmac_init(&hmac, &key_context, 0);
            br_hmac_update(&hmac, current, sizeof(current));
            br_hmac_out(&hmac, current);
            for (uint32_t i = 0; i < HASH_SIZE; ++i)
            {
                result[i] ^= current[i];
            }
        }

        Memory::memcpy(output, result, HASH_SIZE);
        clear_secret(reinterpret_cast<char*>(block), sizeof(block));
        clear_secret(reinterpret_cast<char*>(current), sizeof(current));
        clear_secret(reinterpret_cast<char*>(result), sizeof(result));
        clear_secret(reinterpret_cast<char*>(&key_context), sizeof(key_context));
        clear_secret(reinterpret_cast<char*>(&hmac), sizeof(hmac));
    }

    bool password_matches(const account_t& account, const char* password)
    {
        uint8_t candidate[HASH_SIZE];
        derive_password_hash(password, account.salt, candidate);
        uint8_t difference = 0;
        for (uint32_t i = 0; i < HASH_SIZE; ++i)
        {
            difference |= candidate[i] ^ account.password_hash[i];
        }
        clear_secret(reinterpret_cast<char*>(candidate), sizeof(candidate));
        return difference == 0;
    }

    char hex_digit(uint8_t value)
    {
        return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('a' + value - 10);
    }

    int hex_value(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        return -1;
    }

    bool append_char(char* buffer, uint32_t capacity, uint32_t* length, char value)
    {
        if (*length + 1 >= capacity)
        {
            return false;
        }
        buffer[(*length)++] = value;
        buffer[*length] = '\0';
        return true;
    }

    bool append_text(char* buffer, uint32_t capacity, uint32_t* length, const char* text)
    {
        for (size_t i = 0; text[i] != '\0'; ++i)
        {
            if (!append_char(buffer, capacity, length, text[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool append_hex(char* buffer, uint32_t capacity, uint32_t* length, const uint8_t* bytes, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!append_char(buffer, capacity, length, hex_digit(bytes[i] >> 4)) || !append_char(buffer, capacity, length, hex_digit(bytes[i] & 0x0F)))
            {
                return false;
            }
        }
        return true;
    }

    bool serialize_database(char* buffer, uint32_t capacity, uint32_t* length)
    {
        *length = 0;
        buffer[0] = '\0';
        if (!append_text(buffer, capacity, length, "NASUAUTH2\n") || !append_char(buffer, capacity, length, login_enabled ? '1' : '0') || !append_char(buffer, capacity, length, '\n') || !append_text(buffer, capacity, length, automatic_login) || !append_char(buffer, capacity, length, '\n'))
        {
            return false;
        }

        for (uint32_t i = 0; i < account_count; ++i)
        {
            const account_t& account = accounts[i];
            if (!append_text(buffer, capacity, length, account.name) || !append_char(buffer, capacity, length, '|') || !append_char(buffer, capacity, length, account.administrator ? '1' : '0') || !append_char(buffer, capacity, length, '|') || !append_hex(buffer, capacity, length, account.salt, SALT_SIZE) || !append_char(buffer, capacity, length, '|') || !append_hex(buffer, capacity, length, account.password_hash, HASH_SIZE) || !append_char(buffer, capacity, length, '\n'))
            {
                return false;
            }
        }
        return true;
    }

    bool persist_database()
    {
        if (Disk::storage_is_ram())
        {
            storage_available = false;
            set_status("Only a RAM disk is available; settings will not survive reboot.");
            return false;
        }

        storage_available = clawfs_exists();
        if (!storage_available)
        {
            set_status("ClawFS is not formatted. Run 'format' before saving settings.");
            return false;
        }

        auth_storage_access_guard_t access_guard;
        uint32_t etc_sector = get_sector_by_path("/etc");
        if (etc_sector == 0)
        {
            clawfs_mkdir("/", "etc");
            etc_sector = get_sector_by_path("/etc");
        }
        CLAWFSEntry etc_entry = {};
        if (etc_sector == 0 || find_entry_in_dir(CLAWFS_ROOT_SECTOR, "etc", &etc_entry) == 0 || etc_entry.type != CLAWFS_DIRECTORY)
        {
            set_status("ClawFS has no writable /etc directory.");
            return false;
        }

        char serialized[DATABASE_CAPACITY];
        uint32_t length = 0;
        if (!serialize_database(serialized, sizeof(serialized), &length))
        {
            set_status("Account database is too large.");
            return false;
        }
        const bool saved = clawfs_write_file(DATABASE_PATH, serialized, length);
        clear_secret(serialized, sizeof(serialized));
        if (!saved)
        {
            set_status("ClawFS write failed. Check disk health and free space.");
        }
        return saved;
    }

    void print_save_error(const char* fallback)
    {
        Gpu::print_error(status_message[0] != '\0' ? status_message : fallback);
        Gpu::print("\n");
    }

    bool parse_line(const char* data, uint32_t size, uint32_t* offset, char* line, uint32_t capacity)
    {
        const uint32_t start = *offset;
        uint32_t length = 0;
        while (*offset < size && data[*offset] != '\n')
        {
            if (length + 1 >= capacity)
            {
                return false;
            }
            line[length++] = data[(*offset)++];
        }
        if (*offset < size)
        {
            ++*offset;
        }
        if (length > 0 && line[length - 1] == '\r')
        {
            --length;
        }
        line[length] = '\0';
        return *offset > start;
    }

    bool decode_hex(const char* text, uint8_t* output, uint32_t output_size)
    {
        for (uint32_t i = 0; i < output_size; ++i)
        {
            const int high = hex_value(text[i * 2]);
            const int low = hex_value(text[i * 2 + 1]);
            if (high < 0 || low < 0)
            {
                return false;
            }
            output[i] = static_cast<uint8_t>((high << 4) | low);
        }
        return text[output_size * 2] == '\0';
    }

    bool parse_database(const char* data, uint32_t size, bool* needs_migration)
    {
        uint32_t offset = 0;
        char line[128];
        if (needs_migration == nullptr || !parse_line(data, size, &offset, line, sizeof(line)))
        {
            return false;
        }
        *needs_migration = strcmp(line, "NASUAUTH1") == 0;
        if ((!*needs_migration && strcmp(line, "NASUAUTH2") != 0) || !parse_line(data, size, &offset, line, sizeof(line)) || (strcmp(line, "0") != 0 && strcmp(line, "1") != 0))
        {
            return false;
        }
        login_enabled = line[0] == '1';

        if (!parse_line(data, size, &offset, line, sizeof(line)) || !valid_username(line))
        {
            return false;
        }
        strcpy(automatic_login, line);

        account_count = 0;
        while (offset < size && account_count < MAX_USERS)
        {
            if (!parse_line(data, size, &offset, line, sizeof(line)))
            {
                return false;
            }
            if (line[0] == '\0')
            {
                continue;
            }

            char* first = find_character(line, '|');
            if (first == nullptr)
            {
                return false;
            }
            *first++ = '\0';
            char* second = find_character(first, '|');
            if (second == nullptr)
            {
                return false;
            }
            *second++ = '\0';
            char* third = find_character(second, '|');
            if (third == nullptr)
            {
                return false;
            }
            *third++ = '\0';

            account_t& account = accounts[account_count];
            if (!valid_username(line) || (strcmp(first, "0") != 0 && strcmp(first, "1") != 0) || strlen(second) != SALT_SIZE * 2 || strlen(third) != HASH_SIZE * 2 || find_account(line) >= 0)
            {
                return false;
            }
            strcpy(account.name, line);
            account.administrator = first[0] == '1';
            if (!decode_hex(second, account.salt, SALT_SIZE) || !decode_hex(third, account.password_hash, HASH_SIZE))
            {
                return false;
            }
            ++account_count;
        }

        if (offset != size || account_count == 0 || find_account(automatic_login) < 0)
        {
            return false;
        }
        for (uint32_t i = 0; i < account_count; ++i)
        {
            if (accounts[i].administrator)
            {
                if (*needs_migration)
                {
                    login_enabled = false;
                }
                return true;
            }
        }
        return false;
    }

    bool parse_permission_mode(const char* text, uint16_t* mode)
    {
        if (text == nullptr || mode == nullptr ||
            strlen(text) != 3 ||
            text[0] < '0' || text[0] > '7' ||
            text[1] < '0' || text[1] > '7' ||
            text[2] < '0' || text[2] > '7')
        {
            return false;
        }

        *mode = static_cast<uint16_t>(
            (text[0] - '0') * 64 + (text[1] - '0') * 8 + (text[2] - '0'));
        return true;
    }

    bool load_path_permissions()
    {
        path_permission_count = 0;
        uint32_t size = 0;
        bool file_exists = false;
        {
            auth_storage_access_guard_t access_guard;
            if (!clawfs_get_file_size(PERMISSIONS_PATH, &size))
            {
                file_exists = get_sector_by_path(PERMISSIONS_PATH) != 0;
            }
            else
            {
                file_exists = true;
            }
        }
        if (!file_exists)
        {
            return true;
        }
        if (size == 0 || size >= DATABASE_CAPACITY)
        {
            return false;
        }

        char database[DATABASE_CAPACITY];
        uint32_t read_size = 0;
        bool read_ok = false;
        {
            auth_storage_access_guard_t access_guard;
            read_ok = clawfs_read_file(PERMISSIONS_PATH, database,
                                       sizeof(database), &read_size);
        }
        if (!read_ok || read_size != size)
        {
            clear_secret(database, sizeof(database));
            return false;
        }

        uint32_t offset = 0;
        char line[PATH_CAPACITY + USERNAME_CAPACITY + 8];
        bool valid = parse_line(database, read_size, &offset, line, sizeof(line)) &&
            strcmp(line, "NASUACL1") == 0;

        while (valid && offset < read_size)
        {
            if (!parse_line(database, read_size, &offset, line, sizeof(line)))
            {
                valid = false;
                break;
            }
            if (line[0] == '\0')
            {
                continue;
            }
            if (path_permission_count >= MAX_PATH_PERMISSIONS)
            {
                valid = false;
                break;
            }

            char* owner = find_character(line, '|');
            if (owner == nullptr)
            {
                valid = false;
                break;
            }
            *owner++ = '\0';
            char* mode_text = find_character(owner, '|');
            if (mode_text == nullptr)
            {
                valid = false;
                break;
            }
            *mode_text++ = '\0';

            path_permission_t& permission = path_permissions[path_permission_count];
            if (!valid_username(line) || find_account(line) < 0 ||
                !parse_permission_mode(owner, &permission.mode) ||
                !valid_permission_path(mode_text))
            {
                valid = false;
                break;
            }
            for (uint32_t i = 0; i < path_permission_count; ++i)
            {
                if (strcmp(path_permissions[i].path, mode_text) == 0)
                {
                    valid = false;
                    break;
                }
            }
            if (!valid)
            {
                break;
            }
            strcpy(permission.owner, line);
            strcpy(permission.path, mode_text);
            ++path_permission_count;
        }

        clear_secret(database, sizeof(database));
        if (!valid || offset != read_size)
        {
            path_permission_count = 0;
            return false;
        }
        return true;
    }

    bool persist_path_permissions()
    {
        if (!storage_available)
        {
            return false;
        }

        char database[DATABASE_CAPACITY];
        uint32_t length = 0;
        if (!append_text(database, sizeof(database), &length, "NASUACL1\n"))
        {
            return false;
        }
        for (uint32_t i = 0; i < path_permission_count; ++i)
        {
            const path_permission_t& permission = path_permissions[i];
            const char mode[4] = {
                static_cast<char>('0' + ((permission.mode >> 6) & 7)),
                static_cast<char>('0' + ((permission.mode >> 3) & 7)),
                static_cast<char>('0' + (permission.mode & 7)),
                '\0'
            };
            if (!append_text(database, sizeof(database), &length, permission.owner) ||
                !append_char(database, sizeof(database), &length, '|') ||
                !append_text(database, sizeof(database), &length, mode) ||
                !append_char(database, sizeof(database), &length, '|') ||
                !append_text(database, sizeof(database), &length, permission.path) ||
                !append_char(database, sizeof(database), &length, '\n'))
            {
                return false;
            }
        }

        bool saved = false;
        {
            auth_storage_access_guard_t access_guard;
            saved = clawfs_write_file(PERMISSIONS_PATH, database, length);
        }
        clear_secret(database, sizeof(database));
        return saved;
    }

    bool set_path_permission(const char* path, const char* owner, uint16_t mode)
    {
        int index = find_exact_path_permission(path);
        const bool added = index < 0;
        if (added)
        {
            if (path_permission_count >= MAX_PATH_PERMISSIONS)
            {
                return false;
            }
            index = static_cast<int>(path_permission_count++);
            path_permissions[index] = {};
        }

        const path_permission_t previous = path_permissions[index];
        strcpy(path_permissions[index].path, path);
        strcpy(path_permissions[index].owner, owner);
        path_permissions[index].mode = mode;
        if (persist_path_permissions())
        {
            return true;
        }

        if (added)
        {
            --path_permission_count;
        }
        else
        {
            path_permissions[index] = previous;
        }
        return false;
    }

    void seed_account(const char* name, const char* password, bool administrator)
    {
        account_t& account = accounts[account_count++];
        strcpy(account.name, name);
        account.administrator = administrator;
        generate_salt(account.salt);
        derive_password_hash(password, account.salt, account.password_hash);
    }

    void seed_defaults()
    {
        account_count = 0;
        seed_account("root", "admin", true);
        seed_account("user", "user", false);
        login_enabled = false;
        strcpy(automatic_login, "user");
    }

    void apply_account_session(uint32_t index, bool reset_shell);

    bool reset_invalid_account_database()
    {
        storage_available = !Disk::storage_is_ram() && clawfs_exists();
        if (!storage_available)
        {
            set_status(Disk::storage_is_ram()
                ? "Recovery requires persistent ClawFS storage."
                : "Recovery requires formatted ClawFS storage.");
            return false;
        }

        path_permission_count = 0;
        if (!persist_path_permissions())
        {
            set_status("Could not clear stale filesystem permissions.");
            return false;
        }

        seed_defaults();
        if (!persist_database())
        {
            set_status("Could not save the new account database.");
            return false;
        }

        database_valid = true;
        recovery_armed = false;
        apply_account_session(static_cast<uint32_t>(find_account("user")), true);
        return true;
    }

    void apply_account_session(uint32_t index, bool reset_shell)
    {
        const bool switching_from_existing_session = current_user_valid;
        active_account_index = static_cast<int>(index);
        strcpy(user_name, accounts[index].name);
        current_user_valid = true;
        strcpy(current_path, "/home");
        napp_set_current_path(current_path);
        screen_mode = SCREEN_HIDDEN;
        password_action = PASSWORD_ACTION_NONE;
        switching_user = false;
        input[0] = '\0';
        clear_secret(password_input, sizeof(password_input));
        clear_secret(pending_password, sizeof(pending_password));
        set_status("");
        if (reset_shell)
        {
            Gpu::init_text_buffer();
        }
        else if (switching_from_existing_session)
        {
            Gpu::print_info("User switched successfully.\n");
            Gpu::print_cmd();
        }
    }

    void fail_login()
    {
        clear_secret(password_input, sizeof(password_input));
        set_status("Invalid username or password.");
    }

    bool parse_arguments(const char* args, char* first, size_t first_capacity, char* second, size_t second_capacity)
    {
        while (*args == ' ')
        {
            ++args;
        }
        size_t first_length = 0;
        while (*args != '\0' && *args != ' ')
        {
            if (first_length + 1 >= first_capacity)
            {
                return false;
            }
            first[first_length++] = *args++;
        }
        first[first_length] = '\0';

        while (*args == ' ')
        {
            ++args;
        }
        size_t second_length = 0;
        while (*args != '\0' && *args != ' ')
        {
            if (second_length + 1 >= second_capacity)
            {
                return false;
            }
            second[second_length++] = *args++;
        }
        second[second_length] = '\0';
        while (*args == ' ')
        {
            ++args;
        }
        return *args == '\0';
    }

    void print_usage(const char* usage)
    {
        Gpu::print_error("Usage: ");
        Gpu::print(usage);
        Gpu::print("\n");
    }

    bool require_admin()
    {
        const int index = current_account();
        if (!is_administrator(index))
        {
            Gpu::print_error("Administrator privileges required.\n");
            return false;
        }
        return true;
    }

    bool begin_password_change(password_action_t action, const char* target)
    {
        storage_available = clawfs_exists();
        if (!storage_available || Disk::storage_is_ram())
        {
            Gpu::print_error(Disk::storage_is_ram() ? "A persistent disk is required to change accounts.\n" : "Account changes require a formatted ClawFS disk.\n");
            return false;
        }
        strcpy(password_target, target);
        password_action = action;
        screen_mode = SCREEN_NEW_PASSWORD;
        clear_secret(password_input, sizeof(password_input));
        clear_secret(pending_password, sizeof(pending_password));
        set_status("");
        Gpu::print_info("Enter the new password in the secure prompt.\n");
        return true;
    }

    void commit_password_change()
    {
        const bool adding_user = password_action == PASSWORD_ACTION_ADD_USER;
        const int target_index = find_account(password_target);
        account_t previous_account = {};
        if (adding_user)
        {
            const int current = current_account();
            if (!is_administrator(current))
            {
                Gpu::print_error("Administrator privileges required.\n");
                password_action = PASSWORD_ACTION_NONE;
                screen_mode = SCREEN_HIDDEN;
                clear_secret(password_input, sizeof(password_input));
                clear_secret(pending_password, sizeof(pending_password));
                Gpu::print_cmd();
                return;
            }
            if (target_index >= 0)
            {
                set_status("That account already exists.");
                screen_mode = SCREEN_HIDDEN;
                password_action = PASSWORD_ACTION_NONE;
                return;
            }
            if (account_count >= MAX_USERS)
            {
                set_status("Maximum number of accounts reached.");
                screen_mode = SCREEN_HIDDEN;
                password_action = PASSWORD_ACTION_NONE;
                return;
            }
            account_t added_account = {};
            strcpy(added_account.name, password_target);
            added_account.administrator = false;
            generate_salt(added_account.salt);
            derive_password_hash(pending_password, added_account.salt, added_account.password_hash);
            accounts[account_count++] = added_account;
        }
        else
        {
            if (target_index < 0)
            {
                set_status("The account no longer exists.");
                password_action = PASSWORD_ACTION_NONE;
                screen_mode = SCREEN_HIDDEN;
                return;
            }
            previous_account = accounts[target_index];
            generate_salt(accounts[target_index].salt);
            derive_password_hash(pending_password, accounts[target_index].salt, accounts[target_index].password_hash);
        }

        if (!persist_database())
        {
            if (adding_user)
            {
                --account_count;
            }
            else
            {
                accounts[target_index] = previous_account;
            }
            if (status_message[0] == '\0')
            {
                set_status("Could not save account changes.");
            }
        }
        else
        {
            set_status(adding_user ? "Account created successfully." : "Password changed successfully.");
        }

        password_action = PASSWORD_ACTION_NONE;
        screen_mode = SCREEN_HIDDEN;
        clear_secret(password_input, sizeof(password_input));
        clear_secret(pending_password, sizeof(pending_password));
        Gpu::print_info(status_message);
        Gpu::print("\n");
        if (current_account() >= 0)
        {
            strcpy(user_name, accounts[current_account()].name);
        }
        Gpu::print_cmd();
    }

    void finish_password_entry()
    {
        if (!valid_password(password_input))
        {
            set_status("Password must contain 1 to 63 characters.");
            clear_secret(password_input, sizeof(password_input));
            return;
        }
        strcpy(pending_password, password_input);
        clear_secret(password_input, sizeof(password_input));
        screen_mode = SCREEN_CONFIRM_PASSWORD;
        set_status("");
    }

    void finish_password_confirmation()
    {
        if (strcmp(pending_password, password_input) != 0)
        {
            clear_secret(password_input, sizeof(password_input));
            clear_secret(pending_password, sizeof(pending_password));
            screen_mode = SCREEN_NEW_PASSWORD;
            set_status("Passwords did not match. Try again.");
            return;
        }
        clear_secret(password_input, sizeof(password_input));
        commit_password_change();
    }

    void finish_login()
    {
        const int index = find_account(input);
        if (!database_valid || index < 0 || !password_matches(accounts[index], password_input))
        {
            fail_login();
            return;
        }
        apply_account_session(static_cast<uint32_t>(index), !switching_user);
    }
}

bool auth_init()
{
    const bool clawfs_available = clawfs_exists();
    storage_available = clawfs_available && !Disk::storage_is_ram();
    database_valid = true;
    current_user_valid = false;
    active_account_index = -1;
    switching_user = false;
    password_action = PASSWORD_ACTION_NONE;
    recovery_armed = false;

    if (clawfs_available)
    {
        uint32_t size = 0;
        auth_storage_access_guard_t access_guard;
        if (clawfs_get_file_size(DATABASE_PATH, &size))
        {
            char database[DATABASE_CAPACITY];
            uint32_t read_size = 0;
            bool needs_migration = false;
            database_valid = size < sizeof(database) && clawfs_read_file(DATABASE_PATH, database, sizeof(database), &read_size) && read_size == size && parse_database(database, read_size, &needs_migration);
            clear_secret(database, sizeof(database));
            if (!database_valid)
            {
                set_status("Account DB invalid. Press R for account recovery.");
                screen_mode = SCREEN_DATABASE_RECOVERY;
                input[0] = '\0';
                return false;
            }
            if (needs_migration && !persist_database())
            {
                login_enabled = true;
                set_status("Could not save updated login settings.");
                screen_mode = SCREEN_LOGIN_USERNAME;
                return false;
            }
        }
        else if (get_sector_by_path(DATABASE_PATH) != 0)
        {
            database_valid = false;
            set_status("Account DB invalid. Press R for account recovery.");
            screen_mode = SCREEN_DATABASE_RECOVERY;
            input[0] = '\0';
            return false;
        }
        else
        {
            seed_defaults();
            if (storage_available && !persist_database())
            {
                set_status("Could not initialize the account database.");
                screen_mode = SCREEN_LOGIN_USERNAME;
                return false;
            }
            if (!storage_available)
            {
                set_status(Disk::storage_is_ram() ? "Only a RAM disk is available; settings will not survive reboot." : "ClawFS is not formatted; settings will not survive reboot.");
            }
        }

        if (!load_path_permissions())
        {
            path_permission_count = 0;
            Gpu::print_warn("Permission database invalid; default filesystem permissions remain active.\n");
        }
    }
    else
    {
        seed_defaults();
        set_status(Disk::storage_is_ram() ? "Only a RAM disk is available; settings will not survive reboot." : "ClawFS is not formatted; settings will not survive reboot.");
    }

    if (login_enabled)
    {
        strcpy(input, "user");
        screen_mode = SCREEN_LOGIN_USERNAME;
        return true;
    }

    const int automatic_index = find_account(automatic_login);
    if (automatic_index < 0)
    {
        database_valid = false;
        set_status("Automatic login account does not exist.");
        screen_mode = SCREEN_DATABASE_RECOVERY;
        input[0] = '\0';
        return false;
    }
    apply_account_session(static_cast<uint32_t>(automatic_index), true);
    return true;
}

bool auth_is_capturing_input()
{
    return screen_mode != SCREEN_HIDDEN;
}

void auth_handle_key(char key)
{
    if (!database_valid)
    {
        if (screen_mode != SCREEN_DATABASE_RECOVERY)
        {
            return;
        }
        if (!recovery_armed)
        {
            if (key == 'r' || key == 'R')
            {
                recovery_armed = true;
                input[0] = '\0';
                set_status("Type RESET and press Enter. Accounts will be recreated.");
            }
            return;
        }

        if (key == '\b')
        {
            const size_t length = strlen(input);
            if (length > 0)
            {
                input[length - 1] = '\0';
            }
            return;
        }
        if (key == '\n')
        {
            if (strcmp(input, "RESET") != 0)
            {
                input[0] = '\0';
                set_status("Confirmation did not match. Type RESET or reboot.");
                return;
            }
            reset_invalid_account_database();
            return;
        }
        if (key >= 32 && key <= 126)
        {
            const size_t length = strlen(input);
            if (length + 1 < sizeof(input))
            {
                input[length] = key;
                input[length + 1] = '\0';
            }
        }
        return;
    }

    if (key == '\b')
    {
        if (screen_mode == SCREEN_LOGIN_USERNAME)
        {
            const size_t length = strlen(input);
            if (length > 0)
            {
                input[length - 1] = '\0';
            }
        }
        else if (screen_mode == SCREEN_LOGIN_PASSWORD || screen_mode == SCREEN_NEW_PASSWORD || screen_mode == SCREEN_CONFIRM_PASSWORD)
        {
            const size_t length = strlen(password_input);
            if (length > 0)
            {
                password_input[length - 1] = '\0';
            }
        }
        set_status("");
        return;
    }

    if (key == '\n')
    {
        if (screen_mode == SCREEN_LOGIN_USERNAME)
        {
            if (!valid_username(input) || find_account(input) < 0)
            {
                set_status("Unknown account.");
                return;
            }
            screen_mode = SCREEN_LOGIN_PASSWORD;
            set_status("");
        }
        else if (screen_mode == SCREEN_LOGIN_PASSWORD)
        {
            finish_login();
        }
        else if (screen_mode == SCREEN_NEW_PASSWORD)
        {
            finish_password_entry();
        }
        else if (screen_mode == SCREEN_CONFIRM_PASSWORD)
        {
            finish_password_confirmation();
        }
        return;
    }

    if (key < 32 || key > 126)
    {
        return;
    }

    char* target = screen_mode == SCREEN_LOGIN_USERNAME ? input : password_input;
    const size_t capacity = screen_mode == SCREEN_LOGIN_USERNAME ? sizeof(input) : sizeof(password_input);
    const size_t length = strlen(target);
    if (length + 1 < capacity)
    {
        target[length] = key;
        target[length + 1] = '\0';
        set_status("");
    }
}

bool auth_handle_mouse_click(int x, int y)
{
    if (screen_mode == SCREEN_HIDDEN || Gpu::fb == nullptr)
    {
        return false;
    }

    const int width = static_cast<int>(Gpu::fb->width);
    const int height = static_cast<int>(Gpu::fb->height);
    const int panel_width = width < 600 ? width - 32 : 560;
    const int panel_height = height < 400 ? height - 24 : 360;
    const int left = (width - panel_width) / 2;
    const int top = (height - panel_height) / 2;
    const int hero_width = panel_width < 440 ? 116 : 190;
    const int form_left = left + hero_width + (panel_width < 440 ? 12 : 28);
    const int form_right = left + panel_width - 24;
    const bool compact = panel_height < 340 || panel_width < 440;
    const int button_y = top + (compact ? 174 : 224);
    const int button_height = compact ? 34 : 38;

    if (x >= form_left && x < form_right && y >= button_y &&
        y < button_y + button_height)
    {
        auth_handle_key('\n');
    }
    return true;
}

static uint32_t blend_channel(uint32_t start, uint32_t end, uint32_t step, uint32_t steps, uint32_t shift)
{
    const int32_t first = static_cast<int32_t>((start >> shift) & 0xFF);
    const int32_t last = static_cast<int32_t>((end >> shift) & 0xFF);
    return static_cast<uint32_t>(first + ((last - first) * static_cast<int32_t>(step)) / static_cast<int32_t>(steps));
}

static void draw_outline(int x, int y, int width, int height, uint32_t color)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    Gpu::fill_block(x, y, color, width, 1);
    Gpu::fill_block(x, y + height - 1, color, width, 1);
    Gpu::fill_block(x, y, color, 1, height);
    Gpu::fill_block(x + width - 1, y, color, 1, height);
}

void auth_draw_screen()
{
    if (screen_mode == SCREEN_HIDDEN || Gpu::fb == nullptr)
    {
        return;
    }

    const int width = static_cast<int>(Gpu::fb->width);
    const int height = static_cast<int>(Gpu::fb->height);
    const int panel_width = width < 600 ? width - 32 : 560;
    const int panel_height = height < 400 ? height - 24 : 360;
    const int left = (width - panel_width) / 2;
    const int top = (height - panel_height) / 2;
    const int hero_width = panel_width < 440 ? 116 : 190;
    const int form_left = left + hero_width + (panel_width < 440 ? 12 : 28);
    const int form_right = left + panel_width - 24;
    const int form_width = form_right - form_left;
    const bool compact = panel_height < 340 || panel_width < 440;

    const uint32_t background_top = 0xFF0B1020;
    const uint32_t background_bottom = 0xFF17263A;
    const int background_bands = 48;
    for (int band = 0; band < background_bands; ++band)
    {
        const uint32_t color = (0xFFu << 24) | (blend_channel(background_top, background_bottom, band, background_bands - 1, 16) << 16) | (blend_channel(background_top, background_bottom, band, background_bands - 1, 8) << 8) | blend_channel(background_top, background_bottom, band, background_bands - 1, 0);
        const int y1 = height * band / background_bands;
        const int y2 = height * (band + 1) / background_bands;
        Gpu::fill_block(0, y1, color, width, y2 - y1 + 1);
    }

    Gpu::fill_block(left, top, 0xFFF2F5FA, panel_width, panel_height);
    draw_outline(left, top, panel_width, panel_height, 0xFF51617A);

    const int hero_x = left + 1;
    const int hero_y = top + 1;
    const int hero_height = panel_height - 2;
    const int hero_bands = 24;
    for (int band = 0; band < hero_bands; ++band)
    {
        const uint32_t hero_top = 0xFF142437;
        const uint32_t hero_bottom = 0xFF0D1726;
        const uint32_t color = (0xFFu << 24) | (blend_channel(hero_top, hero_bottom, band, hero_bands - 1, 16) << 16) | (blend_channel(hero_top, hero_bottom, band, hero_bands - 1, 8) << 8) | blend_channel(hero_top, hero_bottom, band, hero_bands - 1, 0);
        const int y1 = hero_y + hero_height * band / hero_bands;
        const int y2 = hero_y + hero_height * (band + 1) / hero_bands;
        Gpu::fill_block(hero_x, y1, color, hero_width - 1, y2 - y1 + 1);
    }

    Gpu::fill_block(left + hero_width - 3, top + 28, 0xFF45C4B0, 3, panel_height - 56);
    if (hero_width > 150)
    {
        Gpu::fill_block(left + 16, top + 32, 0xFF45C4B0, 34, 34);
        Gpu::print_at8("N", left + 29, top + 45, 0xFF10202A);
        Gpu::print_at8("NasuaOS", left + 58, top + 45, 0xFFFFFFFF);
    }
    else
    {
        Gpu::fill_block(left + 26, top + 32, 0xFF45C4B0, 42, 42);
        Gpu::print_at16("N", left + 39, top + 44, 0xFF10202A);
        Gpu::print_at8("NasuaOS", left + 78, top + 48, 0xFFFFFFFF);
    }

    const char* title = "Welcome back";
    const char* subtitle = "Sign in to continue to your desktop.";
    const char* field_label = "USERNAME";
    const char* button_label = "CONTINUE";
    if (screen_mode == SCREEN_NEW_PASSWORD)
    {
        title = password_action == PASSWORD_ACTION_ADD_USER ? "Create account" : "New password";
        subtitle = password_action == PASSWORD_ACTION_ADD_USER ? "Choose a password for this account." : "Choose a strong new password.";
        field_label = "NEW PASSWORD";
        button_label = "CONTINUE";
    }
    else if (screen_mode == SCREEN_CONFIRM_PASSWORD)
    {
        title = password_action == PASSWORD_ACTION_ADD_USER ? "Confirm account" : "Confirm password";
        subtitle = "Enter the same password again to confirm.";
        field_label = "CONFIRM PASSWORD";
        button_label = "SAVE PASSWORD";
    }
    else if (screen_mode == SCREEN_LOGIN_PASSWORD)
    {
        field_label = "PASSWORD";
        button_label = "SIGN IN";
        if (switching_user)
        {
            subtitle = "Signing in as a different user.";
        }
    }
    else if (screen_mode == SCREEN_DATABASE_RECOVERY)
    {
        title = "Account recovery";
        subtitle = recovery_armed
            ? "This recreates the default accounts."
            : "Invalid database. Press R to begin recovery.";
        field_label = "TYPE RESET";
        button_label = "RESET ACCOUNTS";
    }
    const int label_y = top + (compact ? 112 : 151);
    const int field_y = top + (compact ? 128 : 169);
    const int field_height = compact ? 32 : 38;
    const int button_y = top + (compact ? 174 : 224);
    const int button_height = compact ? 34 : 38;
    const int helper_y = top + (compact ? 216 : 277);

    Gpu::print_at8_clipped("ACCOUNT ACCESS", form_left, top + (compact ? 20 : 34), 0xFF568A8A, form_left, form_right);
    if (compact || panel_width < 440)
    {
        Gpu::print_at8_clipped(title, form_left, top + (compact ? 46 : 66), 0xFF182333, form_left, form_right);
    }
    else
    {
        Gpu::print_at16(title, form_left, top + 62, 0xFF182333);
    }
    Gpu::print_at8_clipped(subtitle, form_left, top + (compact ? 68 : 92), 0xFF65758A, form_left, form_right);

    if (screen_mode == SCREEN_LOGIN_PASSWORD)
    {
        Gpu::print_at8("USER", form_left, top + (compact ? 91 : 120), 0xFF8290A2);
        Gpu::print_at8_clipped(input, form_left + 40, top + (compact ? 91 : 120), 0xFF243247, form_left + 40, form_right);
    }
    Gpu::print_at8(field_label, form_left, label_y, 0xFF56677D);
    Gpu::fill_block(form_left, field_y, 0xFFE1E7EF, form_width, field_height);
    Gpu::fill_block(form_left + 1, field_y + 1, 0xFFFFFFFF, form_width - 2, field_height - 2);
    draw_outline(form_left, field_y, form_width, field_height, 0xFFCBD4E0);

    if (screen_mode == SCREEN_LOGIN_USERNAME)
    {
        Gpu::print_at8_clipped(input, form_left + 12, field_y + (compact ? 12 : 15), 0xFF243247, form_left + 12, form_right - 8);
    }
    else if (screen_mode == SCREEN_DATABASE_RECOVERY)
    {
        Gpu::print_at8_clipped(input, form_left + 12, field_y + (compact ? 12 : 15), 0xFF243247, form_left + 12, form_right - 8);
    }
    else
    {
        char masked[PASSWORD_CAPACITY];
        size_t length = strlen(password_input);
        const size_t visible_capacity = form_width > 28 ? static_cast<size_t>((form_width - 28) / 8) : 1;
        if (length > visible_capacity)
        {
            length = visible_capacity;
        }
        if (length >= sizeof(masked))
        {
            length = sizeof(masked) - 1;
        }
        for (size_t i = 0; i < length; ++i)
        {
            masked[i] = '*';
        }
        masked[length] = '\0';
        Gpu::print_at8(masked, form_left + 12, field_y + (compact ? 12 : 15), 0xFF243247);
        clear_secret(masked, sizeof(masked));
    }

    Gpu::fill_block(form_left, button_y, 0xFF167F7A, form_width, button_height);
    Gpu::fill_block(form_left, button_y, 0xFF219A8D, form_width, 3);
    Gpu::print_at8_clipped(button_label, form_left + 14, button_y + (compact ? 13 : 15), 0xFFFFFFFF, form_left + 14, form_right);
    Gpu::print_at8_clipped(status_message[0] != '\0' ? status_message : "Press Enter to continue", form_left, helper_y, status_message[0] != '\0' ? 0xFFB04450 : 0xFF7C899A, form_left, form_right);
}

bool auth_execute_command(const char* command, const char* args)
{
    if (strcmp(command, "chmod") == 0)
    {
        char mode_text[4];
        char requested_path[PATH_CAPACITY];
        char path[PATH_CAPACITY];
        uint16_t mode = 0;
        if (!parse_arguments(args, mode_text, sizeof(mode_text),
                             requested_path, sizeof(requested_path)) ||
            !parse_permission_mode(mode_text, &mode) ||
            !resolve_permission_path(requested_path, path))
        {
            print_usage("chmod <000-777> <path>");
            return true;
        }

        const char* owner = nullptr;
        const int existing = find_path_permission(path);
        if (existing >= 0)
        {
            owner = path_permissions[existing].owner;
        }
        else
        {
            owner = home_directory_owner(path);
        }

        const int current = current_account();
        if (current < 0 ||
            (!is_administrator(current) &&
             (owner == nullptr || strcmp(owner, user_name) != 0)))
        {
            Gpu::print_error("chmod: administrator or directory owner required.\n");
            return true;
        }
        if (get_sector_by_path(path) == 0)
        {
            Gpu::print_error("chmod: path not found.\n");
            return true;
        }
        if (owner == nullptr)
        {
            owner = user_name;
        }

        char owner_copy[USERNAME_CAPACITY];
        strcpy(owner_copy, owner);
        if (!set_path_permission(path, owner_copy, mode))
        {
            Gpu::print_error("chmod: could not save permissions.\n");
            return true;
        }
        Gpu::print_info("Permissions updated.\n");
        return true;
    }

    if (strcmp(command, "chown") == 0)
    {
        char owner[USERNAME_CAPACITY];
        char requested_path[PATH_CAPACITY];
        char path[PATH_CAPACITY];
        if (!parse_arguments(args, owner, sizeof(owner), requested_path,
                             sizeof(requested_path)) ||
            !valid_username(owner) ||
            !resolve_permission_path(requested_path, path))
        {
            print_usage("chown <user> <path>");
            return true;
        }
        if (!require_admin())
        {
            return true;
        }
        if (find_account(owner) < 0)
        {
            Gpu::print_error("chown: unknown user.\n");
            return true;
        }
        if (get_sector_by_path(path) == 0)
        {
            Gpu::print_error("chown: path not found.\n");
            return true;
        }

        uint16_t mode = 0700;
        const int existing = find_path_permission(path);
        if (existing >= 0)
        {
            mode = path_permissions[existing].mode;
        }
        else if (home_directory_owner(path) == nullptr)
        {
            mode = 0755;
        }
        if (!set_path_permission(path, owner, mode))
        {
            Gpu::print_error("chown: could not save permissions.\n");
            return true;
        }
        Gpu::print_info("Owner updated.\n");
        return true;
    }

    if (strcmp(command, "permissions") == 0)
    {
        char requested_path[PATH_CAPACITY];
        char path[PATH_CAPACITY];
        char extra[2];
        if (!parse_arguments(args, requested_path, sizeof(requested_path),
                             extra, sizeof(extra)) ||
            extra[0] != '\0')
        {
            print_usage("permissions [path]");
            return true;
        }
        if (requested_path[0] == '\0')
        {
            strcpy(requested_path, current_path);
        }
        if (!resolve_permission_path(requested_path, path))
        {
            Gpu::print_error("permissions: invalid path.\n");
            return true;
        }
        if (current_account() < 0)
        {
            Gpu::print_error("No authenticated user.\n");
            return true;
        }
        if (!auth_check_path_permission(path, false))
        {
            Gpu::print_error("Permission denied.\n");
            return true;
        }
        if (get_sector_by_path(path) == 0)
        {
            Gpu::print_error("permissions: path not found.\n");
            return true;
        }

        const int effective_permission = find_path_permission(path);
        const char* owner = effective_permission >= 0
            ? path_permissions[effective_permission].owner
            : home_directory_owner(path);
        const uint16_t mode = effective_permission >= 0
            ? path_permissions[effective_permission].mode
            : static_cast<uint16_t>(owner != nullptr ? 0700 :
                (path_is_within(path, "/tmp") ||
                 path_is_within(path, "/mnt") ||
                 path_is_within(path, "/dev") ? 0777 : 0755));
        Gpu::print(path);
        Gpu::print(" owner=");
        Gpu::print(owner != nullptr ? owner : "root");
        Gpu::print(" mode=");
        Gpu::print_num8((mode >> 6) & 7);
        Gpu::print_num8((mode >> 3) & 7);
        Gpu::print_num8(mode & 7);
        Gpu::print("\n");
        return true;
    }

    if (strcmp(command, "users") == 0)
    {
        while (*args == ' ')
        {
            ++args;
        }
        if (*args != '\0')
        {
            print_usage("users");
            return true;
        }
        if (current_account() < 0)
        {
            Gpu::print_error("No authenticated user.\n");
            return true;
        }

        Gpu::print_info("User accounts:\n");
        for (uint32_t index = 0; index < account_count; ++index)
        {
            Gpu::print(" - ");
            Gpu::print(accounts[index].name);
            if (is_administrator(static_cast<int>(index)))
            {
                Gpu::print(" (administrator)");
            }
            Gpu::print("\n");
        }
        return true;
    }

    if (strcmp(command, "login") == 0)
    {
        if (strcmp(args, "--on") == 0)
        {
            if (!require_admin())
            {
                return true;
            }
            const bool previous = login_enabled;
            login_enabled = true;
            set_status("");
            if (!persist_database())
            {
                login_enabled = previous;
                print_save_error("Could not save login settings.");
                return true;
            }
            Gpu::print_info("Login screen enabled for the next boot.\n");
        }
        else if (strcmp(args, "--off") == 0)
        {
            if (!require_admin())
            {
                return true;
            }
            const bool previous = login_enabled;
            char previous_auto[USERNAME_CAPACITY];
            strcpy(previous_auto, automatic_login);
            login_enabled = false;
            strcpy(automatic_login, user_name);
            set_status("");
            if (!persist_database())
            {
                login_enabled = previous;
                strcpy(automatic_login, previous_auto);
                print_save_error("Could not save login settings.");
                return true;
            }
            Gpu::print_info("Login screen disabled; automatic login is enabled.\n");
        }
        else if (args[0] == '\0')
        {
            switching_user = true;
            strcpy(input, user_name);
            clear_secret(password_input, sizeof(password_input));
            screen_mode = SCREEN_LOGIN_PASSWORD;
            set_status("");
            Gpu::print_info("Enter your password in the secure prompt.\n");
        }
        else
        {
            print_usage("login --on | --off");
        }
        return true;
    }

    if (strcmp(command, "su") == 0)
    {
        char name[USERNAME_CAPACITY];
        char extra[2];
        if (!parse_arguments(args, name, sizeof(name), extra, sizeof(extra)) || !valid_username(name) || extra[0] != '\0')
        {
            print_usage("su <username>");
            return true;
        }
        if (find_account(name) < 0)
        {
            Gpu::print_error("Unknown account.\n");
            return true;
        }
        strcpy(input, name);
        clear_secret(password_input, sizeof(password_input));
        switching_user = true;
        screen_mode = SCREEN_LOGIN_PASSWORD;
        set_status("");
        Gpu::print_info("Enter the account password in the secure prompt.\n");
        return true;
    }

    if (strcmp(command, "useradd") == 0)
    {
        char name[USERNAME_CAPACITY];
        char extra[2];
        if (!parse_arguments(args, name, sizeof(name), extra, sizeof(extra)) || !valid_username(name) || extra[0] != '\0')
        {
            print_usage("useradd <username>");
            return true;
        }
        if (!require_admin())
        {
            return true;
        }
        if (find_account(name) >= 0)
        {
            Gpu::print_error("That account already exists.\n");
            return true;
        }
        if (account_count >= MAX_USERS)
        {
            Gpu::print_error("Maximum number of accounts reached.\n");
            return true;
        }
        begin_password_change(PASSWORD_ACTION_ADD_USER, name);
        return true;
    }

    if (strcmp(command, "userdel") == 0)
    {
        char name[USERNAME_CAPACITY];
        char extra[2];
        if (!parse_arguments(args, name, sizeof(name), extra, sizeof(extra)) || !valid_username(name) || extra[0] != '\0')
        {
            print_usage("userdel <username>");
            return true;
        }
        if (!require_admin())
        {
            return true;
        }

        const int target = find_account(name);
        if (target < 0)
        {
            Gpu::print_error("Unknown account.\n");
            return true;
        }
        if (strcmp(accounts[target].name, "root") == 0)
        {
            Gpu::print_error("The root account cannot be deleted.\n");
            return true;
        }
        const int current = current_account();
        if (target == current)
        {
            Gpu::print_error("You cannot delete the currently logged-in account.\n");
            return true;
        }
        if (Disk::storage_is_ram() || !clawfs_exists())
        {
            Gpu::print_error(Disk::storage_is_ram() ? "A persistent disk is required to change accounts.\n" : "Account changes require a formatted ClawFS disk.\n");
            return true;
        }

        account_t removed_account = accounts[target];
        char previous_auto[USERNAME_CAPACITY];
        strcpy(previous_auto, automatic_login);
        const int previous_active_index = active_account_index;

        for (uint32_t index = static_cast<uint32_t>(target); index + 1 < account_count; ++index)
        {
            accounts[index] = accounts[index + 1];
        }
        --account_count;
        accounts[account_count] = {};
        if (strcmp(automatic_login, name) == 0)
        {
            strcpy(automatic_login, "root");
        }
        if (target < active_account_index)
        {
            --active_account_index;
        }

        if (!persist_database())
        {
            for (uint32_t index = account_count; index > static_cast<uint32_t>(target); --index)
            {
                accounts[index] = accounts[index - 1];
            }
            accounts[target] = removed_account;
            ++account_count;
            strcpy(automatic_login, previous_auto);
            active_account_index = previous_active_index;
            print_save_error("Could not save account changes.");
            return true;
        }

        Gpu::print_info("Account deleted successfully.\n");
        return true;
    }

    if (strcmp(command, "passwd") == 0)
    {
        char name[USERNAME_CAPACITY];
        char extra[2];
        if (!parse_arguments(args, name, sizeof(name), extra, sizeof(extra)) || extra[0] != '\0')
        {
            print_usage("passwd [username]");
            return true;
        }
        const int current = current_account();
        if (current < 0)
        {
            Gpu::print_error("No authenticated user.\n");
            return true;
        }
        if (name[0] == '\0')
        {
            strcpy(name, user_name);
        }
        const int target = find_account(name);
        if (target < 0)
        {
            Gpu::print_error("Unknown account.\n");
            return true;
        }
        if (!is_administrator(current) && target != current)
        {
            Gpu::print_error("You may only change your own password.\n");
            return true;
        }
        begin_password_change(PASSWORD_ACTION_CHANGE_PASSWORD, name);
        return true;
    }

    if (strcmp(command, "usermod") == 0)
    {
        char old_name[USERNAME_CAPACITY];
        char new_name[USERNAME_CAPACITY];
        if (!parse_arguments(args, old_name, sizeof(old_name), new_name, sizeof(new_name)) || !valid_username(old_name) || !valid_username(new_name))
        {
            print_usage("usermod <current-name> <new-name>");
            return true;
        }

        const int current = current_account();
        const int target = find_account(old_name);
        if (current < 0 || target < 0)
        {
            Gpu::print_error("Unknown account.\n");
            return true;
        }
        if (!is_administrator(current) && target != current)
        {
            Gpu::print_error("You may only rename your own account.\n");
            return true;
        }
        if (strcmp(old_name, new_name) == 0)
        {
            Gpu::print_info("Account name is unchanged.\n");
            return true;
        }
        if (find_account(new_name) >= 0)
        {
            Gpu::print_error("That account name is already in use.\n");
            return true;
        }
        if (!storage_available)
        {
            Gpu::print_error(Disk::storage_is_ram() ? "A persistent disk is required to change accounts.\n" : "Account changes require a formatted ClawFS disk.\n");
            return true;
        }

        char old_auto[USERNAME_CAPACITY];
        strcpy(old_auto, automatic_login);
        strcpy(accounts[target].name, new_name);
        if (strcmp(automatic_login, old_name) == 0)
        {
            strcpy(automatic_login, new_name);
        }
        if (!persist_database())
        {
            strcpy(accounts[target].name, old_name);
            strcpy(automatic_login, old_auto);
            Gpu::print_error("Could not save account changes.\n");
            return true;
        }
        if (target == current)
        {
            strcpy(user_name, new_name);
        }
        Gpu::print_info("Account renamed successfully.\n");
        return true;
    }

    return false;
}

const char* auth_current_username()
{
    return current_account() >= 0 ? user_name : nullptr;
}

bool auth_current_user_is_administrator()
{
    return is_administrator(current_account());
}

bool auth_check_path_permission(const char* path, bool write)
{
    if (!valid_permission_path(path))
    {
        return false;
    }

    if (internal_auth_storage_access &&
        auth_storage_path_allowed(path, write))
    {
        return true;
    }

    const int account_index = current_account();
    if (account_index < 0)
    {
        return false;
    }
    if (auth_current_user_is_administrator())
    {
        return true;
    }

    const int permission_index = find_path_permission(path);
    for (uint32_t i = 0; i < path_permission_count; ++i)
    {
        const path_permission_t& ancestor = path_permissions[i];
        if (strcmp(path, ancestor.path) == 0 ||
            !path_is_within(path, ancestor.path))
        {
            continue;
        }
        const uint16_t ancestor_bits = strcmp(ancestor.owner, user_name) == 0
            ? static_cast<uint16_t>((ancestor.mode >> 6) & 7)
            : static_cast<uint16_t>(ancestor.mode & 7);
        if ((ancestor_bits & 1) == 0)
        {
            return false;
        }
    }

    if (permission_index >= 0)
    {
        const path_permission_t& permission = path_permissions[permission_index];
        const uint16_t access_bits = strcmp(permission.owner, user_name) == 0
            ? static_cast<uint16_t>((permission.mode >> 6) & 7)
            : static_cast<uint16_t>(permission.mode & 7);
        return (access_bits & (write ? 2 : 4)) != 0;
    }

    if (path_is_within(path, "/home") && strcmp(path, "/home") != 0)
    {
        const char* owner = path + 6;
        size_t owner_length = 0;
        while (owner[owner_length] != '\0' && owner[owner_length] != '/')
        {
            ++owner_length;
        }
        return owner_length == strlen(user_name) &&
            strncmp(owner, user_name, owner_length) == 0;
    }

    if (write)
    {
        return path_is_within(path, "/tmp") ||
            path_is_within(path, "/mnt") ||
            path_is_within(path, "/dev");
    }

    return !path_is_within(path, "/etc");
}
