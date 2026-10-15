#pragma once

#include <stdint.h>

namespace SuaScript
{
    using Output = void (*)(char);

    bool compile_and_run(const char* source, uint16_t length, Output output);
}