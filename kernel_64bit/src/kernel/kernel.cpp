#include <cstdint>
#include <cstddef>
#include <limine.h>

#include "system/drivers/drivers.hpp"
#include "system/interrupts/interrupts.hpp"
#include "system/filesystem/filesystem.hpp"

#include "applications/shell/commands.hpp"

#include "system/applications/napp/napp.hpp"

#include "system/gui/vars/colors.hpp"
#include "system/gui/icons/icons.hpp"
#include "system/gui/gui.hpp"

#include "system/sysfunc/logger/logger.hpp"
#include "kernel/kernel_panic/kernel_panic.hpp"

#include "libs/libc/libc.hpp"
#include "libs/asm/asm.hpp"
#include "libs/qr_code/qr_code.hpp"

// DEBUG VARS
bool debug_mode = false;
bool safe_mode = false;
bool kernel_panicked = false;

// Rootfs image handed over by the bootloader as a module.
static const void* rootfs_module_address = nullptr;
static uint64_t rootfs_module_size = 0;

__attribute__((used, section(".limine_requests")))
volatile limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .flags = 0
};

// LIMINE
namespace {
    __attribute__((used, section(".limine_requests")))
    volatile std::uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

    __attribute__((used, section(".limine_requests")))
    volatile limine_framebuffer_request framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };

    __attribute__((used, section(".limine_requests_start")))
    volatile std::uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

    __attribute__((used, section(".limine_requests_end")))
    volatile std::uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

    __attribute__((used, section(".limine_requests")))
    volatile limine_bootloader_info_request bootloader_info_request = {
        .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };

    __attribute__((used, section(".limine_requests")))
    volatile limine_module_request module_request = {
        .id = LIMINE_MODULE_REQUEST_ID,
        .revision = 0,
        .response = nullptr,
        .internal_module_count = 0,
        .internal_modules = nullptr
    };
}

void iqu_init()
{
    Uart::init();

    init_cpu_cores();
    memory_init();
    paging_init();
    pmm_init();
    vmm_init();
    heap_init();
    interrupts_controller_init();
    idt_init();
    pit_init();

    asm volatile("sti");

    storage_init();

    pci_init();
    usb_init();

    mouse_init();

    image_init();

    napp_init(rootfs_module_address, rootfs_module_size);

    if(safe_mode) 
    {
        execute_command("safe_mode");
        Uart::puts("SAFE MODE ENABLED\n");
    }

    if(debug_mode)
    {
        execute_command("debug --on");
        Uart::puts("DEBUG MODE ENABLED");
    }
}

// Initializers
void pre_check()
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) 
    {
        hcf();
    }

    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1) 
    {
        hcf();
    }

    if (module_request.response != nullptr) 
    {
        for (std::uint64_t i = 0; i < module_request.response->module_count; ++i) 
        {
            struct limine_file* file = module_request.response->modules[i];

            if (find_in_string(file->path, "rootfs.img"))
            {
                rootfs_module_address = file->address;
                rootfs_module_size = file->size;
            }
            else if (find_in_string(file->path, "boot_config.txt"))
            {
                const char* content = (const char*)file->address;

                if (content != nullptr)
                {
                    if (find_in_string(content, "SAFE_MODE"))
                    {
                        safe_mode = true;
                    }

                    if (find_in_string(content, "DEBUG"))
                    {
                        debug_mode = true;
                    }
                }
            }
            else if (find_in_string(file->path, "boot_config_sf.txt"))
            {
                const char* content = (const char*)file->address;

                if (content != nullptr)
                {
                    if (find_in_string(content, "SAFE_MODE"))
                    {
                        safe_mode = true;
                    }

                    if (find_in_string(content, "DEBUG"))
                    {
                        debug_mode = true;
                    }
                }
            }
        }
    }
}

// KMAIN
extern "C" void kmain() 
{
    pre_check();

    fb = framebuffer_request.response->framebuffers[0];
    init_backbuffer(fb->width, fb->height, fb->pitch);
    init_text_buffer();

    iqu_init();

    // Main loop
    for (;;) 
    {
        while (inb(0x64) & 1) 
        {
            handle_keyboard();
        }
        
        if(redraw && !kernel_panicked) 
        {
            redraw = false;

            clear_screen();

            draw_background();

            update_gui();
            update_windows_gui();

            handle_mouse();

            render_frame();
        }

        asm volatile("hlt");
    }
}

// CRT
extern "C" {
    int __cxa_atexit(void (*)(void*), void*, void*) 
    { 
        return 0; 
    }

    void __cxa_pure_virtual() 
    { 
        hcf(); 
    }

    void* __dso_handle;
}

extern void (*__init_array[])();
extern void (*__init_array_end[])();