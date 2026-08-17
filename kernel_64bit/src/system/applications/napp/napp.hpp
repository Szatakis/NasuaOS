#pragma once

#include <stdint.h>

#include <napp.h>

#define NAPP_MAX_APPLICATIONS 32
#define NAPP_MAX_NAME 64
#define NAPP_MAX_WINDOWS 8

// Mounts the rootfs image supplied by the bootloader as a module.
void napp_init(const void* rootfs_image, uint64_t rootfs_size);

bool napp_rootfs_available();

// Lists every application found in the rootfs /bin folder.
uint32_t napp_list(char names[][NAPP_MAX_NAME], uint32_t max_names);

bool napp_exists(const char* name);

// Loads /bin/<name>/<name>.napp and runs it. Returns false when it cannot be started.
bool napp_run(const char* name, int* exit_code);
