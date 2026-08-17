#pragma once

#include <stdint.h>

// NasuaOS application (.napp) binary format.
//
// A .napp file is a flat binary image: a napp_header at offset 0, directly
// followed by the application code, data and read only data. The kernel loads
// the whole image into memory and calls the entry point with a pointer to the
// napp_api structure.

#define NAPP_MAGIC 0x5050414E // 'N' 'A' 'P' 'P'
#define NAPP_ABI_VERSION 1
#define NAPP_HEADER_SIZE 64
#define NAPP_NAME_LENGTH 32

struct napp_header
{
    uint32_t magic;
    uint32_t abi_version;
    uint32_t header_size;
    uint32_t entry_offset;

    char name[NAPP_NAME_LENGTH];

    uint32_t reserved[4];
} __attribute__((packed));

// Services the kernel exposes to a running application.
struct napp_api
{
    uint32_t abi_version;

    void (*print)(const char* text);
    void (*print_line)(const char* text);
    void (*print_dec)(uint32_t value);
    void (*print_hex)(uint32_t value);
    void (*sleep_ms)(uint32_t milliseconds);
    void (*serial_log)(const char* text);
};

typedef int (*napp_entry)(const struct napp_api* api);

// Emits the header and pins the entry point right behind it.
#define NAPP_APPLICATION(app_name)                                             \
    extern "C" __attribute__((section(".napp_entry"), used))                   \
    int _start(const napp_api* api);                                           \
                                                                               \
    __attribute__((section(".napp_header"), used))                             \
    const napp_header napp_application_header =                                \
    {                                                                          \
        NAPP_MAGIC,                                                            \
        NAPP_ABI_VERSION,                                                      \
        NAPP_HEADER_SIZE,                                                      \
        NAPP_HEADER_SIZE,                                                      \
        app_name,                                                              \
        { 0, 0, 0, 0 }                                                         \
    }
