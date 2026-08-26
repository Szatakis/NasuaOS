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

    Cpu::init_cores();
    Memory::init();
    Memory::paging::init();
    Memory::pmm::init();
    Memory::vmm::init();
    Memory::heap::init();
    Apic::controller_init();
    idt_init();
    Timer::pit_init();

    asm volatile("sti");

    Disk::init();

    Pci::init();
    Usb::init();

    Mouse::init();

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

    static framebuffer_info fb_info;
    limine_framebuffer* limine_fb = framebuffer_request.response->framebuffers[0];
    fb_info.address = (uint64_t)limine_fb->address;
    fb_info.pitch = limine_fb->pitch;
    fb_info.width = limine_fb->width;
    fb_info.height = limine_fb->height;
    fb_info.bpp = limine_fb->bpp;
    Gpu::fb = &fb_info;
    Gpu::init_backbuffer(fb_info.width, fb_info.height, fb_info.pitch);
    Gpu::init_text_buffer();

    iqu_init();

    // Main loop
    for (;;) 
    {
        while (inb(0x64) & 1) 
        {
            Keyboard::handle_keyboard();
        }
        
        if(Timer::redraw && !kernel_panicked) 
        {
            Timer::redraw = false;

            Gpu::clear_screen(COLOR_NASUA_BG);

            draw_background();

            Gpu::update_gui();
            update_windows_gui();

            draw_start_menu();

            napp_update_ticks();

            Gpu::handle_mouse();

            Gpu::render_frame();
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