#include "napp.hpp"

#include "system/drivers/gpu/driver.hpp"
#include "system/drivers/memory/driver.hpp"
#include "system/drivers/timer/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "system/filesystem/fat/fat.hpp"

#include "system/sysfunc/logger/logger.hpp"

#include "libs/libc/libc.hpp"

#define NAPP_DIRECTORY "/bin"
#define NAPP_EXTENSION ".napp"
#define NAPP_MAX_IMAGE_SIZE (1024 * 1024)

static fat_volume rootfs_volume;
static bool rootfs_mounted = false;

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

static const napp_api kernel_napp_api =
{
    NAPP_ABI_VERSION,
    napp_api_print,
    napp_api_print_line,
    napp_api_print_dec,
    napp_api_print_hex,
    napp_api_sleep_ms,
    napp_api_serial_log
};

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

    Uart::puts("[NAPP] Starting application: ");
    Uart::puts(name);
    Uart::puts("\n");
    log(INFO, "NAPP", "Starting application");

    napp_entry entry = (napp_entry)((uint8_t*)image + header->entry_offset);

    int result = entry(&kernel_napp_api);

    if (exit_code != nullptr)
    {
        *exit_code = result;
    }

    kfree(image);

    Uart::puts("[NAPP] Application finished\n");
    log(INFO, "NAPP", "Application finished");

    return true;
}
