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

// Loads a flat binary from an absolute FAT rootfs path and runs it.
// Used by the shell to execute /bin/<name> and /sbin/<name> commands.
bool napp_run_path(const char* path, int argc, const char* const* argv, int* exit_code);
bool napp_exists_path(const char* path);

// Loads /bin/<name>/<name>.napp and runs it. Returns false when it cannot be started.
bool napp_run(const char* name, int* exit_code);

// Lists flat-binary commands found in the rootfs /sbin folder.
// Optionally fills descriptions[] with each command's description (read from
// the NAPP header). If a binary lacks a description, the default
// "No description available." is written.
uint32_t napp_list_sbin(char names[][NAPP_MAX_NAME], char descriptions[][NAPP_MAX_NAME], uint32_t max_names);

// Path sync between shell's current_path and napp runtime
const char* napp_get_current_path(void);
void napp_set_current_path(const char* path);
