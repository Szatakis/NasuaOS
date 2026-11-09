#include "system/gui/gui.hpp"
#include "system/gui/vars/colors.hpp"
#include "drivers/gpu/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "system/filesystem/fat/fat.hpp"
#include "system/sysfunc/logger/logger.hpp"
#include "libs/png/png.hpp"

extern fat_volume rootfs_volume;
extern bool rootfs_mounted;

namespace
{
    constexpr char WALLPAPER_PATH[] = "/home/user/images/background_dark.png";

    uint32_t* background_buffer = nullptr;
    size_t background_pitch = 0;
    bool initialized = false;

    void* wallpaper_allocate(size_t size)
    {
        return Memory::heap::kmalloc(size);
    }

    void wallpaper_deallocate(void* memory)
    {
        Memory::heap::kfree(memory);
    }

    void report_wallpaper_error(const char* message)
    {
        Uart::puts("[GUI] ");
        Uart::puts(message);
        Uart::puts("\n");
        log(WARN, "GUI", message);
    }

    bool load_wallpaper()
    {
        if (!rootfs_mounted)
        {
            report_wallpaper_error("Root filesystem unavailable; using solid background");
            return false;
        }

        fat_entry_info info{};
        if (!fat_stat(&rootfs_volume, WALLPAPER_PATH, &info) || info.directory ||
            info.size == 0)
        {
            report_wallpaper_error("Wallpaper PNG missing or invalid; using solid background");
            return false;
        }

        uint8_t* png = static_cast<uint8_t*>(Memory::heap::kmalloc(info.size));
        if (png == nullptr)
        {
            report_wallpaper_error("Unable to allocate wallpaper PNG buffer");
            return false;
        }

        uint32_t read_size = 0;
        if (!fat_read_file(&rootfs_volume, WALLPAPER_PATH, png, info.size, &read_size) ||
            read_size != info.size)
        {
            Memory::heap::kfree(png);
            report_wallpaper_error("Unable to read wallpaper PNG");
            return false;
        }

        const Nasua::Png::Allocator allocator =
        {
            wallpaper_allocate,
            wallpaper_deallocate
        };
        Nasua::Png::Image decoded{};
        if (!Nasua::Png::decode(png, info.size, allocator, &decoded))
        {
            Memory::heap::kfree(png);
            report_wallpaper_error("PNG wallpaper decoding failed");
            return false;
        }
        Memory::heap::kfree(png);

        const size_t width = Gpu::fb->width;
        const size_t height = Gpu::fb->height;
        for (size_t y = 0; y < height; ++y)
        {
            const uint32_t source_y = static_cast<uint32_t>(
                static_cast<uint64_t>(y) * decoded.height / height);
            for (size_t x = 0; x < width; ++x)
            {
                const uint32_t source_x = static_cast<uint32_t>(
                    static_cast<uint64_t>(x) * decoded.width / width);
                const uint64_t source_offset =
                    (static_cast<uint64_t>(source_y) * decoded.width + source_x) * 4;
                const uint8_t* rgba = decoded.rgba + source_offset;
                background_buffer[y * background_pitch + x] =
                    (static_cast<uint32_t>(rgba[3]) << 24) |
                    (static_cast<uint32_t>(rgba[0]) << 16) |
                    (static_cast<uint32_t>(rgba[1]) << 8) |
                    static_cast<uint32_t>(rgba[2]);
            }
        }

        Nasua::Png::release(&decoded, allocator);
        Uart::puts("[GUI] Decoded PNG wallpaper from rootfs\n");
        log(INFO, "GUI", "Decoded PNG wallpaper from rootfs");
        return true;
    }
}

size_t bar_h = 36;
size_t bar_h_scaled = 0;

void image_init()
{
    if (initialized)
    {
        return;
    }
    initialized = true;

    if (Gpu::fb == nullptr)
    {
        report_wallpaper_error("Framebuffer unavailable; skipping wallpaper setup");
        return;
    }

    const size_t width = Gpu::fb->width;
    const size_t height = Gpu::fb->height;
    background_pitch = Gpu::get_backbuffer_pitch();
    if (width == 0 || height == 0 || background_pitch < width ||
        height > Gpu::get_backbuffer_height())
    {
        report_wallpaper_error("Unsupported framebuffer dimensions");
        return;
    }

    if (height > static_cast<size_t>(-1) / background_pitch / sizeof(uint32_t))
    {
        report_wallpaper_error("Framebuffer dimensions overflow background buffer");
        return;
    }
    const size_t background_size = background_pitch * height * sizeof(uint32_t);
    background_buffer = static_cast<uint32_t*>(Memory::heap::kmalloc(background_size));
    if (background_buffer == nullptr)
    {
        report_wallpaper_error("Unable to allocate background buffer");
        return;
    }

    bar_h_scaled = (bar_h * height) / 720;
    if (bar_h > bar_h_scaled)
    {
        bar_h = bar_h + bar_h_scaled;
        bar_h_scaled = bar_h - bar_h_scaled;
        bar_h = bar_h - bar_h_scaled;
    }

    for (size_t y = 0; y < height; ++y)
    {
        for (size_t x = 0; x < background_pitch; ++x)
        {
            background_buffer[y * background_pitch + x] = COLOR_NASUA_BG;
        }
    }

    load_wallpaper();
}

void draw_background()
{
    if (Gpu::fb == nullptr || background_buffer == nullptr)
    {
        return;
    }

    uint32_t* backbuffer = Gpu::get_backbuffer();
    if (backbuffer == nullptr)
    {
        return;
    }

    for (size_t y = 0; y < Gpu::fb->height; ++y)
    {
        Memory::memcpy(backbuffer + y * background_pitch,
                       background_buffer + y * background_pitch,
                       Gpu::fb->width * sizeof(uint32_t));
    }
}
