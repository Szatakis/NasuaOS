#include <napp.h>

NAPP_APPLICATION("download", "Download files from HTTPS repositories", false);

namespace
{
    constexpr uint32_t response_capacity = 65536;
    constexpr uint32_t max_file_size = 60000;
    constexpr char catalog_header[] = "NASUA-FILE-CATALOG-1";
    constexpr char download_directory[] = "/home/user/downloads";
    constexpr uint32_t source_host_capacity = 96;
    constexpr uint32_t source_path_capacity = 192;
    char response[response_capacity];

    struct RepoSource
    {
        char host[source_host_capacity];
        char path[source_path_capacity];
    };

    struct FileEntry
    {
        char name[28];
        char version[24];
        char sha256[65];
        char path[160];
    };

    bool strings_equal(const char* left, const char* right)
    {
        if (left == nullptr || right == nullptr)
        {
            return false;
        }
        for (uint32_t index = 0;; ++index)
        {
            if (left[index] != right[index])
            {
                return false;
            }
            if (left[index] == '\0')
            {
                return true;
            }
        }
    }

    uint32_t string_length(const char* text)
    {
        uint32_t length = 0;
        while (text[length] != '\0')
        {
            ++length;
        }
        return length;
    }

    bool is_name_character(char character)
    {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') ||
               character == '-' || character == '_' || character == '.';
    }

    bool copy_field(char* destination, uint32_t capacity,
                    const char* source, uint32_t length)
    {
        if (length == 0 || length >= capacity)
        {
            return false;
        }
        for (uint32_t index = 0; index < length; ++index)
        {
            destination[index] = source[index];
        }
        destination[length] = '\0';
        return true;
    }

    bool valid_source_path(const char* path, bool require_repo_path)
    {
        if (path == nullptr || *path == '\0')
        {
            return false;
        }
        uint32_t slash_count = 0;
        uint32_t component_length = 0;
        bool component_is_dots = true;
        for (uint32_t index = 0;; ++index)
        {
            const char character = path[index];
            if (character == '/' || character == '\0')
            {
                if (component_length == 0 ||
                    (component_is_dots && component_length <= 2))
                {
                    return false;
                }
                if (character == '\0')
                {
                    return !require_repo_path || slash_count == 1;
                }
                ++slash_count;
                if (require_repo_path && slash_count > 1)
                {
                    return false;
                }
                component_length = 0;
                component_is_dots = true;
                continue;
            }
            if (!is_name_character(character))
            {
                return false;
            }
            ++component_length;
            component_is_dots = component_is_dots && character == '.';
        }
    }

    bool parse_repo_link(const char* link, RepoSource& source)
    {
        constexpr char scheme[] = "https://";
        for (uint32_t index = 0; index < sizeof(scheme) - 1; ++index)
        {
            if (link[index] == '\0' || link[index] != scheme[index])
            {
                return false;
            }
        }

        uint32_t host_length = 0;
        uint32_t index = sizeof(scheme) - 1;
        while (link[index] != '\0' && link[index] != '/')
        {
            const char character = link[index];
            if (!((character >= 'a' && character <= 'z') ||
                  (character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') ||
                  character == '.' || character == '-'))
            {
                return false;
            }
            if (host_length + 1 >= sizeof(source.host))
            {
                return false;
            }
            source.host[host_length++] = character;
            ++index;
        }
        if (host_length == 0 || link[index] != '/')
        {
            return false;
        }
        source.host[host_length] = '\0';

        uint32_t path_length = 0;
        ++index;
        while (link[index] != '\0')
        {
            if (path_length + 1 >= sizeof(source.path))
            {
                return false;
            }
            source.path[path_length++] = link[index++];
        }
        while (path_length > 0 && source.path[path_length - 1] == '/')
        {
            --path_length;
        }
        source.path[path_length] = '\0';
        if (!valid_source_path(source.path, strings_equal(source.host, "github.com")))
        {
            return false;
        }

        if (strings_equal(source.host, "github.com"))
        {
            const uint32_t repo_start = string_length(source.path);
            if (repo_start >= 4 &&
                source.path[repo_start - 4] == '.' &&
                source.path[repo_start - 3] == 'g' &&
                source.path[repo_start - 2] == 'i' &&
                source.path[repo_start - 1] == 't')
            {
                source.path[repo_start - 4] = '\0';
                if (!valid_source_path(source.path, true))
                {
                    return false;
                }
            }
            const char raw_host[] = "raw.githubusercontent.com";
            for (uint32_t host_index = 0; host_index < sizeof(raw_host); ++host_index)
            {
                source.host[host_index] = raw_host[host_index];
            }
        }
        return true;
    }

    bool valid_branch(const char* branch)
    {
        if (branch == nullptr || *branch == '\0')
        {
            return false;
        }
        for (uint32_t index = 0; branch[index] != '\0'; ++index)
        {
            if (!is_name_character(branch[index]))
            {
                return false;
            }
        }
        return true;
    }

    bool valid_file_path(const char* path)
    {
        if (path[0] != 'f' || path[1] != 'i' || path[2] != 'l' ||
            path[3] != 'e' || path[4] != 's' || path[5] != '/')
        {
            return false;
        }
        uint32_t component_length = 0;
        bool component_is_dots = true;
        bool has_filename_character = false;
        for (uint32_t index = 6; path[index] != '\0'; ++index)
        {
            const char character = path[index];
            if (character == '/')
            {
                if (component_length == 0 ||
                    (component_is_dots && component_length <= 2))
                {
                    return false;
                }
                component_length = 0;
                component_is_dots = true;
                continue;
            }
            if (!is_name_character(character))
            {
                return false;
            }
            ++component_length;
            component_is_dots = component_is_dots && character == '.';
            has_filename_character = true;
        }
        return has_filename_character && component_length != 0 &&
               !(component_is_dots && component_length <= 2);
    }

    bool parse_entry(const char* line, uint32_t length, FileEntry& entry)
    {
        const char* fields[4] = {};
        uint32_t field_lengths[4] = {};
        uint32_t field_index = 0;
        uint32_t field_start = 0;
        for (uint32_t index = 0; index <= length; ++index)
        {
            if (index == length || line[index] == '|')
            {
                if (field_index >= 4)
                {
                    return false;
                }
                fields[field_index] = line + field_start;
                field_lengths[field_index] = index - field_start;
                ++field_index;
                field_start = index + 1;
            }
        }
        if (field_index != 4 ||
            !copy_field(entry.name, sizeof(entry.name), fields[0], field_lengths[0]) ||
            !copy_field(entry.version, sizeof(entry.version), fields[1], field_lengths[1]) ||
            field_lengths[2] != 64 ||
            !copy_field(entry.sha256, sizeof(entry.sha256), fields[2], field_lengths[2]) ||
            !copy_field(entry.path, sizeof(entry.path), fields[3], field_lengths[3]))
        {
            return false;
        }

        for (uint32_t index = 0; entry.name[index] != '\0'; ++index)
        {
            if (!is_name_character(entry.name[index]))
            {
                return false;
            }
        }
        if (strings_equal(entry.name, ".") || strings_equal(entry.name, ".."))
        {
            return false;
        }
        for (uint32_t index = 0; entry.version[index] != '\0'; ++index)
        {
            if (!is_name_character(entry.version[index]) && entry.version[index] != '+')
            {
                return false;
            }
        }
        for (uint32_t index = 0; index < 64; ++index)
        {
            const char character = entry.sha256[index];
            if (!((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f')))
            {
                return false;
            }
        }
        return valid_file_path(entry.path);
    }

    bool get_http_body(int32_t response_length, const char*& body, uint32_t& body_length)
    {
        if (response_length <= 0)
        {
            return false;
        }
        const uint32_t length = static_cast<uint32_t>(response_length);
        if (length < 12 || response[0] != 'H' || response[1] != 'T' ||
            response[2] != 'T' || response[3] != 'P')
        {
            return false;
        }
        uint32_t status_start = 0;
        while (status_start < length && response[status_start] != ' ')
        {
            ++status_start;
        }
        if (status_start + 4 >= length || response[status_start + 1] != '2' ||
            response[status_start + 2] != '0' || response[status_start + 3] != '0' ||
            response[status_start + 4] != ' ')
        {
            return false;
        }
        for (uint32_t index = 0; index + 3 < length; ++index)
        {
            if (response[index] == '\r' && response[index + 1] == '\n' &&
                response[index + 2] == '\r' && response[index + 3] == '\n')
            {
                body = response + index + 4;
                body_length = length - index - 4;
                return true;
            }
        }
        return false;
    }

    int32_t fetch_file(const napp_api* api, const RepoSource& source, const char* branch,
                       const char* file_path, const char*& body, uint32_t& body_length)
    {
        char request_path[384] = {};
        uint32_t offset = 0;
        const char* parts[] = {"/", source.path, "/", branch, "/", file_path};
        for (uint32_t part = 0; part < sizeof(parts) / sizeof(parts[0]); ++part)
        {
            for (const char* text = parts[part]; *text != '\0'; ++text)
            {
                if (offset + 1 >= sizeof(request_path))
                {
                    api->print_error("Download URL is too long.\n");
                    return -1;
                }
                request_path[offset++] = *text;
            }
        }
        request_path[offset] = '\0';

        const int32_t response_length = api->https_get(
            source.host, 443, request_path,
            response, sizeof(response), 30000);
        if (response_length < 0)
        {
            api->print_error("HTTPS download failed (error ");
            api->print_dec(static_cast<uint32_t>(-response_length));
            api->print_line(").");
            if (response_length == NAPP_HTTPS_ERROR_ENTROPY)
            {
                api->print_error("The VM must expose CPU RDRAND; try QEMU with -cpu max.\n");
            }
            else if (response_length == NAPP_HTTPS_ERROR_CERTIFICATE)
            {
                api->print_error("The server certificate was rejected; check the system date.\n");
            }
            else if (response_length == NAPP_HTTPS_ERROR_CLOCK)
            {
                api->print_error("HTTPS needs a valid system RTC date and time.\n");
            }
            return response_length;
        }
        if (!get_http_body(response_length, body, body_length))
        {
            api->print_error("The GitHub repository returned an invalid HTTP response.\n");
            return -1;
        }
        return response_length;
    }

    bool sha256(const uint8_t* input, uint32_t length, uint8_t digest[32])
    {
        static const uint32_t constants[64] = {
            0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
            0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
            0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
            0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
            0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
            0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
            0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
            0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
            0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
            0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
            0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
            0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
            0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
            0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
            0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
            0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
        };
        uint32_t state[8] = {
            0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
            0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
        };
        const uint64_t bit_length = static_cast<uint64_t>(length) * 8;
        const uint32_t padded_length = ((length + 9 + 63) / 64) * 64;
        if (padded_length > response_capacity)
        {
            return false;
        }

        uint8_t block[64];
        for (uint32_t offset = 0; offset < padded_length; offset += sizeof(block))
        {
            for (uint32_t index = 0; index < sizeof(block); ++index)
            {
                const uint32_t position = offset + index;
                if (position < length)
                {
                    block[index] = input[position];
                }
                else if (position == length)
                {
                    block[index] = 0x80;
                }
                else if (position >= padded_length - 8)
                {
                    block[index] = static_cast<uint8_t>(
                        bit_length >> ((padded_length - 1 - position) * 8));
                }
                else
                {
                    block[index] = 0;
                }
            }
            uint32_t words[64];
            for (uint32_t index = 0; index < 16; ++index)
            {
                words[index] = (static_cast<uint32_t>(block[index * 4]) << 24) |
                               (static_cast<uint32_t>(block[index * 4 + 1]) << 16) |
                               (static_cast<uint32_t>(block[index * 4 + 2]) << 8) |
                               block[index * 4 + 3];
            }
            for (uint32_t index = 16; index < 64; ++index)
            {
                const uint32_t x = words[index - 15];
                const uint32_t y = words[index - 2];
                const uint32_t sigma0 = ((x >> 7) | (x << 25)) ^
                                        ((x >> 18) | (x << 14)) ^ (x >> 3);
                const uint32_t sigma1 = ((y >> 17) | (y << 15)) ^
                                        ((y >> 19) | (y << 13)) ^ (y >> 10);
                words[index] = words[index - 16] + sigma0 +
                               words[index - 7] + sigma1;
            }

            uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
            uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
            for (uint32_t index = 0; index < 64; ++index)
            {
                const uint32_t sum1 = ((e >> 6) | (e << 26)) ^
                                      ((e >> 11) | (e << 21)) ^
                                      ((e >> 25) | (e << 7));
                const uint32_t choose = (e & f) ^ (~e & g);
                const uint32_t temp1 = h + sum1 + choose + constants[index] + words[index];
                const uint32_t sum0 = ((a >> 2) | (a << 30)) ^
                                      ((a >> 13) | (a << 19)) ^
                                      ((a >> 22) | (a << 10));
                const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t temp2 = sum0 + majority;
                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }
        for (uint32_t index = 0; index < 8; ++index)
        {
            digest[index * 4] = static_cast<uint8_t>(state[index] >> 24);
            digest[index * 4 + 1] = static_cast<uint8_t>(state[index] >> 16);
            digest[index * 4 + 2] = static_cast<uint8_t>(state[index] >> 8);
            digest[index * 4 + 3] = static_cast<uint8_t>(state[index]);
        }
        return true;
    }

    bool verify_sha256(const uint8_t* data, uint32_t length, const char* expected)
    {
        uint8_t digest[32];
        if (!sha256(data, length, digest))
        {
            return false;
        }
        static const char hex[] = "0123456789abcdef";
        for (uint32_t index = 0; index < sizeof(digest); ++index)
        {
            if (expected[index * 2] != hex[digest[index] >> 4] ||
                expected[index * 2 + 1] != hex[digest[index] & 0x0F])
            {
                return false;
            }
        }
        return true;
    }

    bool load_catalog(const napp_api* api, const RepoSource& source, const char* branch,
                      const char*& body, uint32_t& body_length)
    {
        if (fetch_file(api, source, branch, "catalog.txt", body, body_length) < 0)
        {
            return false;
        }
        for (uint32_t index = 0; index < body_length; ++index)
        {
            if (body[index] == '\0')
            {
                api->print_error("The file catalog is not valid text.\n");
                return false;
            }
        }
        return true;
    }

    bool for_each_entry(const napp_api* api, const char* body, uint32_t body_length,
                        const char* requested_name, bool list_entries,
                        FileEntry* match, uint32_t* match_count)
    {
        uint32_t line_start = 0;
        bool header_seen = false;
        uint32_t entry_count = 0;
        while (line_start < body_length)
        {
            uint32_t line_end = line_start;
            while (line_end < body_length && body[line_end] != '\n')
            {
                ++line_end;
            }
            uint32_t line_length = line_end - line_start;
            if (line_length > 0 && body[line_start + line_length - 1] == '\r')
            {
                --line_length;
            }
            if (line_length != 0)
            {
                if (!header_seen)
                {
                    if (line_length != sizeof(catalog_header) - 1)
                    {
                        api->print_error("Unsupported file catalog format.\n");
                        return false;
                    }
                    for (uint32_t index = 0; index < line_length; ++index)
                    {
                        if (body[line_start + index] != catalog_header[index])
                        {
                            api->print_error("Unsupported file catalog format.\n");
                            return false;
                        }
                    }
                    header_seen = true;
                }
                else
                {
                    FileEntry entry = {};
                    if (!parse_entry(body + line_start, line_length, entry))
                    {
                        api->print_error("The file catalog contains an invalid entry.\n");
                        return false;
                    }
                    ++entry_count;
                    if (list_entries)
                    {
                        api->print(" - ");
                        api->print(entry.name);
                        api->print(" (");
                        api->print(entry.version);
                        api->print_line(")");
                    }
                    else if (strings_equal(entry.name, requested_name))
                    {
                        *match = entry;
                        ++*match_count;
                    }
                }
            }
            line_start = line_end + 1;
        }
        if (!header_seen)
        {
            api->print_error("The file catalog is empty.\n");
            return false;
        }
        if (list_entries && entry_count == 0)
        {
            api->print_line("No files are available.");
        }
        return true;
    }

    bool ensure_download_directory(const napp_api* api)
    {
        const int type = api->clawfs_get_entry_type("/home/user", "downloads");
        if (type == 1)
        {
            return true;
        }
        if (type != -1)
        {
            api->print_error("/home/user/downloads exists but is not a directory.\n");
            return false;
        }
        api->clawfs_mkdir("/home/user", "downloads");
        if (api->clawfs_get_entry_type("/home/user", "downloads") != 1)
        {
            api->print_error("Could not create /home/user/downloads.\n");
            return false;
        }
        return true;
    }

    void print_usage(const napp_api* api)
    {
        api->print_line("Usage:");
        api->print_line("  download --link https://HOST/PATH [--branch BRANCH] list");
        api->print_line("  download --link https://HOST/PATH [--branch BRANCH] get NAME");
        api->print_line("Files are saved to /home/user/downloads.");
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->abi_version != NAPP_ABI_VERSION ||
        api->https_get == nullptr || api->clawfs_write_file == nullptr ||
        api->clawfs_get_entry_type == nullptr || api->clawfs_mkdir == nullptr)
    {
        if (api != nullptr)
        {
            api->print_error("Download requires NAPP ABI 6, HTTPS, and ClawFS services.\n");
        }
        return 1;
    }

    const char* link = nullptr;
    const char* branch = "main";
    const char* action = nullptr;
    const char* requested_name = nullptr;
    bool branch_set = false;
    for (int index = 1; index < api->argc; ++index)
    {
        const char* argument = api->argv[index];
        if (strings_equal(argument, "--link"))
        {
            if (link != nullptr || index + 1 >= api->argc)
            {
                print_usage(api);
                return 1;
            }
            link = api->argv[++index];
        }
        else if (strings_equal(argument, "--branch"))
        {
            if (branch_set || index + 1 >= api->argc)
            {
                print_usage(api);
                return 1;
            }
            branch = api->argv[++index];
            branch_set = true;
        }
        else if (action == nullptr)
        {
            action = argument;
        }
        else if (strings_equal(action, "get") && requested_name == nullptr)
        {
            requested_name = argument;
        }
        else
        {
            print_usage(api);
            return 1;
        }
    }

    RepoSource source = {};
    if (link == nullptr || !parse_repo_link(link, source) || !valid_branch(branch))
    {
        api->print_error("Specify a valid HTTPS repository link, such as https://github.com/OWNER/REPO.\n");
        print_usage(api);
        return 1;
    }
    if ((!strings_equal(action, "list") && !strings_equal(action, "get")) ||
        (strings_equal(action, "list") && requested_name != nullptr) ||
        (strings_equal(action, "get") && requested_name == nullptr))
    {
        print_usage(api);
        return 1;
    }

    const char* catalog = nullptr;
    uint32_t catalog_length = 0;
    if (!load_catalog(api, source, branch, catalog, catalog_length))
    {
        return 1;
    }
    if (strings_equal(action, "list"))
    {
        api->print_line("Available files:");
        return for_each_entry(api, catalog, catalog_length, nullptr, true, nullptr, nullptr)
            ? 0 : 1;
    }

    FileEntry selected = {};
    uint32_t match_count = 0;
    if (!for_each_entry(api, catalog, catalog_length, requested_name, false,
                        &selected, &match_count))
    {
        return 1;
    }
    if (match_count == 0)
    {
        api->print_error("That file is not listed in the catalog.\n");
        return 1;
    }
    if (match_count != 1)
    {
        api->print_error("The file catalog contains duplicate names for that file.\n");
        return 1;
    }
    if (string_length(selected.name) >= 28)
    {
        api->print_error("The destination filename must be shorter than 28 characters.\n");
        return 1;
    }
    if (!ensure_download_directory(api))
    {
        return 1;
    }
    if (api->clawfs_get_entry_type(download_directory, selected.name) != -1)
    {
        api->print_error("A file with that name already exists in /home/user/downloads.\n");
        return 1;
    }

    const char* file_data = nullptr;
    uint32_t file_length = 0;
    if (fetch_file(api, source, branch, selected.path, file_data, file_length) < 0)
    {
        return 1;
    }
    if (file_length == 0 || file_length > max_file_size)
    {
        api->print_error("Downloaded file is empty or exceeds the 60,000-byte limit.\n");
        return 1;
    }
    if (!verify_sha256(reinterpret_cast<const uint8_t*>(file_data), file_length,
                       selected.sha256))
    {
        api->print_error("Downloaded file SHA-256 does not match the catalog.\n");
        return 1;
    }

    char destination[192] = "/home/user/downloads/";
    uint32_t destination_length = string_length(destination);
    for (uint32_t index = 0; selected.name[index] != '\0'; ++index)
    {
        destination[destination_length++] = selected.name[index];
    }
    destination[destination_length] = '\0';
    if (!api->clawfs_write_file(destination, file_data, file_length))
    {
        api->print_error("Failed to save the file to /home/user/downloads.\n");
        return 1;
    }
    api->print("Downloaded ");
    api->print(selected.name);
    api->print(" (version ");
    api->print(selected.version);
    api->print_line(") to /home/user/downloads.");
    return 0;
}
