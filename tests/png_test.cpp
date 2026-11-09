#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "libs/png/png.hpp"

namespace
{
    size_t allocation_count = 0;
    size_t failed_allocation = static_cast<size_t>(-1);

    void* test_allocate(size_t size)
    {
        if (allocation_count++ == failed_allocation)
        {
            return nullptr;
        }
        return std::malloc(size);
    }

    void test_deallocate(void* memory)
    {
        std::free(memory);
    }

    constexpr uint8_t rgba_png[] =
    {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13,
        73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 2,
        8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0,
        21, 73, 68, 65, 84, 120, 156, 99, 248, 207, 192, 240,
        31, 4, 25, 129, 52, 16, 48, 48, 0, 0, 70, 212,
        7, 250, 210, 94, 108, 124, 0, 0, 0, 0, 73, 69,
        78, 68, 174, 66, 96, 130
    };

    constexpr uint8_t expected_pixels[] =
    {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255
    };
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const Nasua::Png::Allocator allocator = {test_allocate, test_deallocate};
    Nasua::Png::Image image{};
    Nasua::Png::Error error = Nasua::Png::Error::InvalidArgument;

    assert(Nasua::Png::decode(rgba_png, sizeof(rgba_png), allocator, &image, &error));
    assert(error == Nasua::Png::Error::None);
    assert(image.width == 2);
    assert(image.height == 2);
    assert(image.size == sizeof(expected_pixels));
    for (size_t index = 0; index < sizeof(expected_pixels); ++index)
    {
        assert(image.rgba[index] == expected_pixels[index]);
    }
    Nasua::Png::release(&image, allocator);
    assert(image.rgba == nullptr);
    assert(image.width == 0);

    uint8_t invalid_png[sizeof(rgba_png)];
    for (size_t index = 0; index < sizeof(rgba_png); ++index)
    {
        invalid_png[index] = rgba_png[index];
    }
    invalid_png[16] ^= 1;
    assert(!Nasua::Png::decode(invalid_png, sizeof(invalid_png), allocator, &image, &error));
    assert(error == Nasua::Png::Error::InvalidChunk);
    assert(image.rgba == nullptr);

    assert(!Nasua::Png::decode(rgba_png, sizeof(rgba_png), allocator, nullptr, &error));
    assert(error == Nasua::Png::Error::InvalidArgument);

    allocation_count = 0;
    failed_allocation = 3;
    assert(!Nasua::Png::decode(rgba_png, sizeof(rgba_png), allocator, &image, &error));
    assert(error == Nasua::Png::Error::OutOfMemory);
    assert(image.rgba == nullptr);
    failed_allocation = static_cast<size_t>(-1);

    FILE* wallpaper_file = std::fopen(argv[1], "rb");
    assert(wallpaper_file != nullptr);
    assert(std::fseek(wallpaper_file, 0, SEEK_END) == 0);
    const long wallpaper_size = std::ftell(wallpaper_file);
    assert(wallpaper_size > 0);
    assert(std::fseek(wallpaper_file, 0, SEEK_SET) == 0);
    uint8_t* wallpaper = static_cast<uint8_t*>(
        std::malloc(static_cast<size_t>(wallpaper_size)));
    assert(wallpaper != nullptr);
    assert(std::fread(wallpaper, 1, static_cast<size_t>(wallpaper_size), wallpaper_file) ==
           static_cast<size_t>(wallpaper_size));
    assert(std::fclose(wallpaper_file) == 0);

    assert(Nasua::Png::decode(wallpaper, static_cast<size_t>(wallpaper_size),
                              allocator, &image, &error));
    assert(image.width != 0 && image.height != 0);
    assert(image.rgba[3] != 0);
    Nasua::Png::release(&image, allocator);
    std::free(wallpaper);
    return 0;
}
