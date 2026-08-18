#include "napp.hpp"

#include "system/drivers/gpu/driver.hpp"
#include "system/drivers/disk/ata/driver.hpp"
#include "system/drivers/memory/driver.hpp"
#include "system/drivers/timer/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "system/filesystem/fat/fat.hpp"
#include "system/filesystem/clawfs/clawfs.hpp"

#include "system/sysfunc/logger/logger.hpp"
#include "system/vars/info_vars/info_vars.hpp"

#include "libs/libc/libc.hpp"

#define NAPP_DIRECTORY "/bin"
#define NAPP_SBIN_DIRECTORY "/sbin"
#define NAPP_EXTENSION ".napp"
#define NAPP_MAX_IMAGE_SIZE (1024 * 1024)

static fat_volume rootfs_volume;
static bool rootfs_mounted = false;

// Current working directory exposed to napp programs
static char napp_current_path[256] = "/home";

// A graphical application keeps its image mapped for as long as its window
// exists, because the window callbacks live inside that image.
struct napp_window_slot
{
    bool used;

    char owner[NAPP_MAX_NAME];
    char title[NAPP_MAX_NAME];

    window_struct window;
    napp_window view;

    napp_window_draw draw;
    napp_window_key key;
    napp_window_mouse mouse;
};

struct napp_resident_image
{
    bool used;

    char name[NAPP_MAX_NAME];
    void* image;
};

static napp_window_slot window_slots[NAPP_MAX_WINDOWS];
static napp_resident_image resident_images[NAPP_MAX_WINDOWS];

static char loading_application[NAPP_MAX_NAME];
static uint32_t loading_window_count = 0;

static void napp_api_print(const char* text)
{
    if (text != nullptr)
    {
        print(text);
    }
}

static void napp_api_print_line(const char* text)
{
    if (text != nullptr)
    {
        print(text);
        print("\n");
    }
}

static void napp_api_print_dec(uint32_t value)
{
    print_num8(value);
}

static void napp_api_print_hex(uint32_t value)
{
    print_hex(value);
}

static void napp_api_sleep_ms(uint32_t milliseconds)
{
    sleep(milliseconds);
}

static void napp_api_serial_log(const char* text)
{
    if (text != nullptr)
    {
        Uart::puts(text);
    }
}

// ── CWD wrapper ──────────────────────────────────────────────────────────────

static void napp_api_set_cwd(const char* path)
{
    if (path != nullptr)
    {
        uint32_t i = 0;
        while (path[i] && i < 255) { napp_current_path[i] = path[i]; i++; }
        napp_current_path[i] = '\0';
    }
}

// ── ClawFS wrappers (give userspace flat binaries access to the disk FS) ──────

static uint32_t napp_api_clawfs_get_sector(const char* path)
{
    return get_sector_by_path(path);
}

static bool napp_api_clawfs_read_sector(uint32_t sector, void* buffer)
{
    return storage_read_sector(sector, (uint8_t*)buffer);
}

static bool napp_api_clawfs_write_sector(uint32_t sector, const void* buffer)
{
    return storage_write_sector(sector, (uint8_t*)buffer);
}

static void napp_api_clawfs_mkdir(const char* parent_path, const char* dir_name)
{
    clawfs_mkdir(parent_path, dir_name);
}

static void napp_api_clawfs_rm(const char* parent_path, const char* name, uint32_t type)
{
    clawfs_rm(parent_path, name, type);
}

static void napp_api_clawfs_create_file_in(const char* path, const char* name)
{
    clawfs_create_file_in(path, name);
}

static void napp_api_clawfs_dir(const char* path)
{
    clawfs_dir(path);
}

static int napp_api_clawfs_get_entry_type(const char* parent_path, const char* name)
{
    // Walk to parent sector
    uint32_t parent_sector = get_sector_by_path(parent_path);
    if (parent_sector == 0)
    {
        return -1;
    }

    CLAWFSEntry entry;
    if (find_entry_in_dir(parent_sector, name, &entry) != 0)
    {
        return -1;
    }

    return (int)entry.type;
}

static uint32_t napp_api_clawfs_resolve_path(const char* cur_path, const char* rel)
{
    if (rel == nullptr || rel[0] == '\0')
    {
        return get_sector_by_path(cur_path);
    }

    if (rel[0] == '/')
    {
        return get_sector_by_path(rel);
    }

    // Build absolute path
    char abs[256];
    uint32_t i = 0;
    const char* p = cur_path;
    while (*p && i < 254) { abs[i++] = *p++; }
    if (i > 0 && abs[i-1] != '/') { abs[i++] = '/'; }
    const char* r = rel;
    while (*r && i < 254) { abs[i++] = *r++; }
    abs[i] = '\0';

    return get_sector_by_path(abs);
}

static void napp_api_fill_block(int x, int y, uint32_t color, int width, int height)
{
    if (x < 0 || y < 0 || width <= 0 || height <= 0)
    {
        return;
    }

    fill_block((size_t)x, (size_t)y, color, (size_t)width, (size_t)height);
}

static void napp_api_draw_text(const char* text, int x, int y, uint32_t color)
{
    if (text == nullptr || x < 0 || y < 0)
    {
        return;
    }

    print_at8(text, (size_t)x, (size_t)y, color);
}

static int window_title_height(const window_struct* window)
{
    int title = window->height / 10;

    return title < 18 ? 18 : title;
}

static napp_window_slot* slot_of(window_struct* window)
{
    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (window_slots[i].used && &window_slots[i].window == window)
        {
            return &window_slots[i];
        }
    }

    return nullptr;
}

static napp_window_slot* sync_slot(window_struct* window)
{
    napp_window_slot* slot = slot_of(window);

    if (slot == nullptr)
    {
        return nullptr;
    }

    slot->view.pos_x = window->pos_x;
    slot->view.pos_y = window->pos_y;
    slot->view.width = window->width;
    slot->view.height = window->height;
    slot->view.title_height = window_title_height(window);

    return slot;
}

static void napp_window_draw_trampoline(window_struct* window)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->draw != nullptr)
    {
        slot->draw(&slot->view);
    }
}

static void napp_window_key_trampoline(window_struct* window, char key)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->key != nullptr)
    {
        slot->key(&slot->view, key);
    }
}

static void napp_window_mouse_trampoline(window_struct* window, int mouse_x, int mouse_y)
{
    napp_window_slot* slot = sync_slot(window);

    if (slot != nullptr && slot->mouse != nullptr)
    {
        slot->mouse(&slot->view, mouse_x, mouse_y);
    }
}

static napp_window_slot* find_window_of(const char* application)
{
    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (window_slots[i].used && strcmp(window_slots[i].owner, application) == 0)
        {
            return &window_slots[i];
        }
    }

    return nullptr;
}

static void show_window(napp_window_slot* slot)
{
    slot->window.visible = true;
    slot->window.minimized = false;
    slot->window.id = current_id;

    current_id++;

    register_window(&slot->window);
}

static bool napp_api_open_window(const napp_window_config* config)
{
    if (config == nullptr || config->draw == nullptr || loading_application[0] == '\0')
    {
        return false;
    }

    napp_window_slot* slot = nullptr;

    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (!window_slots[i].used)
        {
            slot = &window_slots[i];

            break;
        }
    }

    if (slot == nullptr)
    {
        log(ERROR, "NAPP", "No free window slot");

        return false;
    }

    memset(slot, 0, sizeof(napp_window_slot));

    strcpy(slot->owner, loading_application);
    strcpy(slot->title, config->title != nullptr ? config->title : loading_application);

    slot->draw = config->draw;
    slot->key = config->key;
    slot->mouse = config->mouse;

    slot->view.userdata = config->userdata;

    slot->window.name = slot->title;
    slot->window.pos_x = 10;
    slot->window.pos_y = 10;
    slot->window.width = config->width > 0 ? config->width : 320;
    slot->window.height = config->height > 0 ? config->height : 240;
    slot->window.resizable = config->resizable;
    slot->window.can_maximize = config->can_maximize;
    slot->window.userdata = slot;
    slot->window.draw_content = napp_window_draw_trampoline;
    slot->window.key_press = napp_window_key_trampoline;
    slot->window.mouse_click = napp_window_mouse_trampoline;

    slot->used = true;

    show_window(slot);

    loading_window_count++;

    return true;
}

static const napp_gui kernel_napp_gui =
{
    napp_api_open_window,
    napp_api_fill_block,
    napp_api_draw_text
};

// Updated at the start of each napp_run_path call so the binary sees the
// shell's current working directory.
static napp_api kernel_napp_api =
{
    NAPP_ABI_VERSION,
    napp_api_print,
    napp_api_print_line,
    napp_api_print_dec,
    napp_api_print_hex,
    napp_api_sleep_ms,
    napp_api_serial_log,
    &kernel_napp_gui,
    /* argc */            0,
    /* argv */            nullptr,
    /* current_path */    napp_current_path,
    napp_api_set_cwd,
    napp_api_clawfs_get_sector,
    napp_api_clawfs_read_sector,
    napp_api_clawfs_write_sector,
    napp_api_clawfs_mkdir,
    napp_api_clawfs_rm,
    napp_api_clawfs_create_file_in,
    napp_api_clawfs_dir,
    napp_api_clawfs_get_entry_type,
    napp_api_clawfs_resolve_path
};

static bool remember_image(const char* name, void* image)
{
    for (uint32_t i = 0; i < NAPP_MAX_WINDOWS; i++)
    {
        if (!resident_images[i].used)
        {
            resident_images[i].used = true;
            resident_images[i].image = image;

            strcpy(resident_images[i].name, name);

            return true;
        }
    }

    return false;
}

// Turns "bootcheck.napp" into "bootcheck", returns false for any other file.
static bool strip_extension(const char* file_name, char* output, uint32_t output_size)
{
    uint32_t length = (uint32_t)strlen(file_name);
    uint32_t extension_length = (uint32_t)strlen(NAPP_EXTENSION);

    if (length <= extension_length || length - extension_length >= output_size)
    {
        return false;
    }

    if (!strncmp(file_name + (length - extension_length), NAPP_EXTENSION, extension_length))
    {
        return false;
    }

    for (uint32_t i = 0; i < length - extension_length; i++)
    {
        output[i] = file_name[i];
    }

    output[length - extension_length] = '\0';

    return true;
}

static void build_application_path(const char* name, char* output)
{
    strcpy(output, NAPP_DIRECTORY);
    strcat(output, "/");
    strcat(output, name);
    strcat(output, "/");
    strcat(output, name);
    strcat(output, NAPP_EXTENSION);
}

void napp_init(const void* rootfs_image, uint64_t rootfs_size)
{
    Uart::puts("[NAPP] Initializing...\n");
    log(INFO, "NAPP", "Initializing...");

    rootfs_mounted = false;

    if (rootfs_image == nullptr || rootfs_size == 0)
    {
        Uart::puts("[NAPP] No rootfs module provided\n");
        log(WARN, "NAPP", "No rootfs module provided");

        return;
    }

    if (!fat_mount(&rootfs_volume, rootfs_image, rootfs_size))
    {
        Uart::puts("[NAPP] Failed to mount rootfs\n");
        log(ERROR, "NAPP", "Failed to mount rootfs");

        return;
    }

    rootfs_mounted = true;

    Uart::puts("[NAPP] Rootfs mounted\n");
    log(INFO, "NAPP", "Rootfs mounted");
}

bool napp_rootfs_available()
{
    return rootfs_mounted;
}

uint32_t napp_list(char names[][NAPP_MAX_NAME], uint32_t max_names)
{
    if (!rootfs_mounted || names == nullptr || max_names == 0)
    {
        return 0;
    }

    static fat_entry_info directories[NAPP_MAX_APPLICATIONS];
    static fat_entry_info files[NAPP_MAX_APPLICATIONS];

    uint32_t directory_count = fat_list_directory(&rootfs_volume, NAPP_DIRECTORY, directories, NAPP_MAX_APPLICATIONS);

    uint32_t found = 0;

    for (uint32_t i = 0; i < directory_count && found < max_names; i++)
    {
        if (!directories[i].directory)
        {
            continue;
        }

        char path[256];

        strcpy(path, NAPP_DIRECTORY);
        strcat(path, "/");
        strcat(path, directories[i].name);

        uint32_t file_count = fat_list_directory(&rootfs_volume, path, files, NAPP_MAX_APPLICATIONS);

        for (uint32_t j = 0; j < file_count; j++)
        {
            char application_name[NAPP_MAX_NAME];

            if (files[j].directory)
            {
                continue;
            }

            if (!strip_extension(files[j].name, application_name, NAPP_MAX_NAME))
            {
                continue;
            }

            strcpy(names[found], application_name);

            found++;

            break;
        }
    }

    return found;
}

bool napp_exists(const char* name)
{
    if (!rootfs_mounted || name == nullptr || *name == '\0')
    {
        return false;
    }

    char path[256];

    build_application_path(name, path);

    fat_entry_info info;

    if (!fat_stat(&rootfs_volume, path, &info))
    {
        return false;
    }

    return !info.directory;
}

bool napp_run(const char* name, int* exit_code)
{
    if (!rootfs_mounted || name == nullptr || *name == '\0')
    {
        return false;
    }

    char path[256];

    build_application_path(name, path);

    fat_entry_info info;

    if (!fat_stat(&rootfs_volume, path, &info) || info.directory)
    {
        log(WARN, "NAPP", "Application not found");

        return false;
    }

    if (info.size < NAPP_HEADER_SIZE || info.size > NAPP_MAX_IMAGE_SIZE)
    {
        log(ERROR, "NAPP", "Invalid application size");

        return false;
    }

    void* image = kmalloc(info.size);

    if (image == nullptr)
    {
        log(ERROR, "NAPP", "Out of memory");

        return false;
    }

    uint32_t read_size = 0;

    if (!fat_read_file(&rootfs_volume, path, image, info.size, &read_size) || read_size != info.size)
    {
        log(ERROR, "NAPP", "Failed to read application");

        kfree(image);

        return false;
    }

    const napp_header* header = (const napp_header*)image;

    if (header->magic != NAPP_MAGIC || header->abi_version != NAPP_ABI_VERSION)
    {
        log(ERROR, "NAPP", "Invalid application image");

        kfree(image);

        return false;
    }

    if (header->entry_offset < header->header_size || header->entry_offset >= info.size)
    {
        log(ERROR, "NAPP", "Invalid application entry point");

        kfree(image);

        return false;
    }

    napp_window_slot* existing = find_window_of(name);

    if (existing != nullptr)
    {
        show_window(existing);

        if (exit_code != nullptr)
        {
            *exit_code = 0;
        }

        return true;
    }

    Uart::puts("[NAPP] Starting application: ");
    Uart::puts(name);
    Uart::puts("\n");
    log(INFO, "NAPP", "Starting application");

    napp_entry entry = (napp_entry)((uint8_t*)image + header->entry_offset);

    strcpy(loading_application, name);

    loading_window_count = 0;

    int result = entry(&kernel_napp_api);

    uint32_t opened_windows = loading_window_count;

    loading_application[0] = '\0';
    loading_window_count = 0;

    if (exit_code != nullptr)
    {
        *exit_code = result;
    }

    // A graphical application keeps running through its window callbacks, so
    // its image has to stay mapped.
    if (opened_windows == 0 || !remember_image(name, image))
    {
        kfree(image);
    }

    Uart::puts("[NAPP] Application finished\n");
    log(INFO, "NAPP", "Application finished");

    return true;
}

// ── Path-based flat-binary loader (for /bin/<name> and /sbin/<name>) ──────────

bool napp_exists_path(const char* path)
{
    if (!rootfs_mounted || path == nullptr || *path == '\0')
    {
        return false;
    }

    fat_entry_info info;

    if (!fat_stat(&rootfs_volume, path, &info))
    {
        return false;
    }

    return !info.directory;
}

bool napp_run_path(const char* path, int argc, const char* const* argv, int* exit_code)
{
    if (!rootfs_mounted || path == nullptr || *path == '\0')
    {
        return false;
    }

    fat_entry_info info;

    if (!fat_stat(&rootfs_volume, path, &info) || info.directory)
    {
        log(WARN, "NAPP", "Path not found");
        return false;
    }

    if (info.size == 0 || info.size > NAPP_MAX_IMAGE_SIZE)
    {
        log(ERROR, "NAPP", "Invalid flat binary size");
        return false;
    }

    void* image = kmalloc(info.size);

    if (image == nullptr)
    {
        log(ERROR, "NAPP", "Out of memory");
        return false;
    }

    uint32_t read_size = 0;

    if (!fat_read_file(&rootfs_volume, path, image, info.size, &read_size) || read_size != info.size)
    {
        log(ERROR, "NAPP", "Failed to read flat binary");
        kfree(image);
        return false;
    }

    // Populate runtime fields
    kernel_napp_api.argc = argc;
    kernel_napp_api.argv = argv;
    // current_path already points at napp_current_path (live pointer)

    Uart::puts("[NAPP] Running: ");
    Uart::puts(path);
    Uart::puts("\n");

    int result = 0;

    // Check for NAPP header (sbin commands are NAPP binaries without extension)
    const napp_header* header = (const napp_header*)image;
    if (header->magic == NAPP_MAGIC && header->abi_version == NAPP_ABI_VERSION)
    {
        if (header->entry_offset < header->header_size || header->entry_offset >= info.size)
        {
            log(ERROR, "NAPP", "Invalid application entry point");
            kfree(image);
            return false;
        }

        napp_entry entry = (napp_entry)((uint8_t*)image + header->entry_offset);
        result = entry(&kernel_napp_api);
    }
    else
    {
        // Flat binary (no header)
        typedef int (*flat_entry)(const napp_api*);
        flat_entry entry = (flat_entry)image;
        result = entry(&kernel_napp_api);
    }

    // Reset argc/argv after return
    kernel_napp_api.argc = 0;
    kernel_napp_api.argv = nullptr;

    kfree(image);

    if (exit_code != nullptr)
    {
        *exit_code = result;
    }

    return true;
}

// ── sbin listing ─────────────────────────────────────────────────────────────

uint32_t napp_list_sbin(char names[][NAPP_MAX_NAME], uint32_t max_names)
{
    if (!rootfs_mounted || names == nullptr || max_names == 0)
    {
        return 0;
    }

    static fat_entry_info files[NAPP_MAX_APPLICATIONS];

    uint32_t file_count = fat_list_directory(&rootfs_volume, NAPP_SBIN_DIRECTORY, files, NAPP_MAX_APPLICATIONS);
    uint32_t found = 0;

    for (uint32_t i = 0; i < file_count && found < max_names; i++)
    {
        if (files[i].directory)
        {
            continue;
        }

        // Copy name directly — flat binaries have no extension
        uint32_t j = 0;
        while (files[i].name[j] && j < NAPP_MAX_NAME - 1)
        {
            names[found][j] = files[i].name[j];
            j++;
        }
        names[found][j] = '\0';
        found++;
    }

    return found;
}

const char* napp_get_current_path(void)
{
    return napp_current_path;
}

void napp_set_current_path(const char* path)
{
    if (path != nullptr)
    {
        uint32_t i = 0;
        while (path[i] && i < 255) { napp_current_path[i] = path[i]; i++; }
        napp_current_path[i] = '\0';
    }
}
