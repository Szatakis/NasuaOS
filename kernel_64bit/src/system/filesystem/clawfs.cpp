#include "clawfs.hpp"

#include "system/drivers/video/driver.hpp"
#include "system/drivers/disk/driver.hpp"
#include "system/drivers/memory/driver.hpp"

#include "applications/shell/commands.hpp"

#include "libs/libc/libc.h"

// Funkcje pomocnicze
char* next_path_token(char* str, const char* delim) {
    static char* backup_str = nullptr;
    if (str != nullptr) backup_str = str;
    if (backup_str == nullptr) return nullptr;

    char* start = backup_str;
    while (*backup_str == *delim) { backup_str++; start++; }
    if (*backup_str == '\0') return nullptr;

    while (*backup_str != '\0') {
        if (*backup_str == *delim) {
            *backup_str = '\0';
            backup_str++;
            return start;
        }
        backup_str++;
    }
    return start;
}

// Logika systemu plików

uint32_t find_entry_in_dir(uint32_t dir_sector, const char* name, CLAWFSEntry* out_entry) {
    uint8_t buffer[512];
    disk_read_sector(dir_sector, buffer);
    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++) { // 512 / sizeof(CLAWFSEntry) approx 12
        if (strcmp(entries[i].name, name) == 0) {
            *out_entry = entries[i];
            return entries[i].data_sector;
        }
    }
    return 0;
}

uint32_t next_free_sector = 106; // Zaczynamy od 106, bo 102-105 zajęte przez root
uint32_t get_next_free_sector() { return next_free_sector++; }

uint32_t get_sector_by_path(const char* path) {
    if (strcmp(path, "/") == 0) return CLAWFS_ROOT_SECTOR;
    
    uint32_t current_dir = CLAWFS_ROOT_SECTOR;
    char path_copy[128];
    strcpy(path_copy, path);

    char* token = next_path_token(path_copy, "/");
    CLAWFSEntry entry;

    while (token != nullptr) {
        uint32_t next_sector = find_entry_in_dir(current_dir, token, &entry);
        if (next_sector == 0) return 0;
        
        current_dir = entry.data_sector;
        token = next_path_token(nullptr, "/");
    }
    return current_dir;
}

void setup_entry(CLAWFSEntry* e, const char* name, uint32_t type, uint32_t sector) {
    memclear(e->name, 28); // WAŻNE: Czyścimy całe pole nazwy!
    strcpy(e->name, name);
    e->type = type;
    e->data_sector = sector;
    
    uint8_t zero_buf[512] = {0};
    disk_write_sector(sector, zero_buf);
}

void clawfs_format() {
    // Nagłówek
    uint8_t buffer[512] = {0};
    CLAWFSHeader* header = (CLAWFSHeader*)buffer;
    memcpy(header->signature, "CLAWFS", 6);
    header->version = CLAWFS_VERSION;
    disk_write_sector(CLAWFS_START_LBA, buffer);

    // Root (101)
    uint8_t root_buffer[512] = {0};
    CLAWFSEntry* entries = (CLAWFSEntry*)root_buffer;
    setup_entry(&entries[0], "bin", CLAWFS_DIRECTORY, 102);
    setup_entry(&entries[1], "dev", CLAWFS_DIRECTORY, 103);
    setup_entry(&entries[2], "etc", CLAWFS_DIRECTORY, 104);
    setup_entry(&entries[3], "home", CLAWFS_DIRECTORY, 105);
    setup_entry(&entries[4], "tmp", CLAWFS_DIRECTORY, 106);
    setup_entry(&entries[5], "var", CLAWFS_DIRECTORY, 107);
    disk_write_sector(CLAWFS_ROOT_SECTOR, root_buffer);

    // Podstruktura
    clawfs_create_file_in("/bin", "kernel_bin");
    clawfs_mkdir("/home", "user");
    clawfs_mkdir("/home", "root");
    clawfs_mkdir("/home/user", "desktop");
    clawfs_mkdir("/home/user", "Documents");
    clawfs_mkdir("/home/user", "images");
    clawfs_mkdir("/home/user", "videos");
    
    print_info("Format complete.\n");
}

void clawfs_mkdir(const char* parent_path, const char* dir_name) {
    uint32_t parent_sector = get_sector_by_path(parent_path);
    if (parent_sector == 0) { print_error("Parent not found!\n"); return; }

    uint8_t buffer[512];
    disk_read_sector(parent_sector, buffer);
    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++) {
        if (entries[i].name[0] == '\0') {
            setup_entry(&entries[i], dir_name, CLAWFS_DIRECTORY, get_next_free_sector());
            disk_write_sector(parent_sector, buffer);
            print_info("Dir created.\n");
            return;
        }
    }
}

void clawfs_create_file_in(const char* path, const char* name) {
    uint32_t target_sector = get_sector_by_path(path);
    if (target_sector == 0) { print_error("Path not found!\n"); return; }

    uint8_t buffer[512];
    disk_read_sector(target_sector, buffer);
    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++) {
        if (entries[i].name[0] == '\0') {
            memclear(entries[i].name, 28);
            strcpy(entries[i].name, name);
            entries[i].type = CLAWFS_FILE;
            entries[i].data_sector = get_next_free_sector();
            disk_write_sector(target_sector, buffer);
            print_info("File created.\n");
            return;
        }
    }
}

void clawfs_dir(const char* path) {
    uint32_t target = get_sector_by_path(path);
    if (target == 0) { print_error("Dir not found!\n"); return; }

    uint8_t buffer[512];
    disk_read_sector(target, buffer);
    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;

    for (int i = 0; i < 12; i++) {
        if (entries[i].name[0] != '\0') {
            if (entries[i].type == CLAWFS_DIRECTORY) print("<DIR>  ");
            else print("       ");
            print(entries[i].name);
            print("\n");
        }
    }
}

bool clawfs_exists()
{
    uint8_t buffer[512];
    disk_read_sector(CLAWFS_START_LBA, buffer);

    CLAWFSHeader* header = (CLAWFSHeader*)buffer;

    if (memcmp(header->signature, "CLAWFS", 6) != 0)
        return false;

    if (header->version != CLAWFS_VERSION)
        return false;

    return true;
}

void clawfs_rm(const char* parent_path, const char* name, uint32_t type)
{
    uint32_t parent_sector = get_sector_by_path(parent_path);

    if(parent_sector == 0)
    {
        print_error("Parent directory not found!\n");
        return;
    }


    uint8_t buffer[512];
    disk_read_sector(parent_sector, buffer);

    CLAWFSEntry* entries = (CLAWFSEntry*)buffer;


    for(int i = 0; i < 12; i++)
    {
        if(entries[i].name[0] != '\0')
        {
            if(strcmp(entries[i].name, name) == 0 &&
               entries[i].type == type)
            {
                // sprawdzenie czy folder jest pusty
                if(type == CLAWFS_DIRECTORY)
                {
                    uint8_t dir_buffer[512];
                    disk_read_sector(entries[i].data_sector, dir_buffer);

                    CLAWFSEntry* sub_entries = (CLAWFSEntry*)dir_buffer;

                    for(int j = 0; j < 12; j++)
                    {
                        if(sub_entries[j].name[0] != '\0')
                        {
                            print_error("Directory is not empty!\n");
                            return;
                        }
                    }
                }


                // usunięcie wpisu
                memclear(&entries[i], sizeof(CLAWFSEntry));

                disk_write_sector(parent_sector, buffer);

                print_info("Removed successfully.\n");
                return;
            }
        }
    }

    if(type == CLAWFS_FILE)
    {
        print_error("File not found!\n");
    }
    else if(type == CLAWFS_DIRECTORY)
    {
        print_error("Directory not found!\n");
    }
}