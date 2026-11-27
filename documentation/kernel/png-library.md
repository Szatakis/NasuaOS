# PNG Library

The shared C++ decoder is in `resources/libs/png/`. It decodes a PNG byte buffer
to an 8-bit RGBA image and uses allocator callbacks, so the caller can use it
with either kernel or host memory management.

```cpp
const Nasua::Png::Allocator allocator = {allocate, deallocate};
Nasua::Png::Image image{};
Nasua::Png::Error error;

if (Nasua::Png::decode(data, data_size, allocator, &image, &error))
{
    // Use image.rgba; each pixel contains red, green, blue, alpha bytes.
    Nasua::Png::release(&image, allocator);
}
```

The decoder supports PNG grayscale, truecolor, indexed-color, grayscale-alpha,
and RGBA images; 1-, 2-, 4-, 8-, and 16-bit samples where permitted by PNG;
and Adam7 interlacing. It accepts stored, fixed-Huffman, and dynamic-Huffman
DEFLATE streams. Images are limited to 8192 pixels per dimension and 256 MiB
of decoded image data.

`decode` initializes the output image to empty before decoding. On success, the
caller owns the returned buffer and must pass it to `release` with the same
allocator. On failure, `Error` identifies invalid arguments, malformed input,
unsupported formats, or allocation failures.
