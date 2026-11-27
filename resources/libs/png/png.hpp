#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Nasua
{
    namespace Png
    {
        using Allocate = void* (*)(size_t size);
        using Deallocate = void (*)(void* memory);

        struct Allocator
        {
            Allocate allocate;
            Deallocate deallocate;
        };

        struct Image
        {
            uint32_t width;
            uint32_t height;
            uint8_t* rgba;
            size_t size;
        };

        enum class Error : uint8_t
        {
            None,
            InvalidArgument,
            InvalidSignature,
            InvalidChunk,
            UnsupportedFormat,
            InvalidImageData,
            OutOfMemory
        };

        bool decode(const uint8_t* png, size_t png_size, const Allocator& allocator,
                    Image* image, Error* error = nullptr);
        void release(Image* image, const Allocator& allocator);
    }
}
