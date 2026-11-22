#pragma once

#include <stdint.h>

namespace NasuaAI
{
    bool generate(const char* prompt, char* output,
                  uint32_t output_capacity, uint32_t max_tokens);
}
