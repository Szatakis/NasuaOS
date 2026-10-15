#include "drivers/memory/driver.hpp"

#include <stddef.h>

extern "C" void* memcpy(void* destination, const void* source, size_t length)
{
    return Memory::memcpy(destination, source, length);
}

extern "C" void* memmove(void* destination, const void* source, size_t length)
{
    return Memory::memmove(destination, source, length);
}

extern "C" void* memset(void* destination, int value, size_t length)
{
    return Memory::memset(destination, value, length);
}

extern "C" int memcmp(const void* left, const void* right, size_t length)
{
    return Memory::memcmp(left, right, length);
}

extern "C" size_t strlen(const char* text)
{
    size_t length = 0;
    while (text[length] != '\0')
    {
        ++length;
    }
    return length;
}
