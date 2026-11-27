#pragma once

#include <stdint.h>

namespace NasuaAI
{
    bool generate(const char* prompt, char* output,
                  uint32_t output_capacity, uint32_t max_tokens);

    // Asynchronous generation. generate_start hands the prompt to a
    // scheduler worker task and returns a job id (0 on failure). Poll the
    // job with generate_poll: 0 while running, 1 when finished (result
    // copied into output), -1 on failure. Only one job may run at a time.
    uint32_t generate_start(const char* prompt, uint32_t max_tokens);
    int32_t generate_poll(uint32_t job, char* output,
                          uint32_t output_capacity);

    // Statistics of the most recently finished generation: number of
    // generated tokens and wall-clock milliseconds. Returns false when no
    // completed job has stats recorded yet.
    bool generate_stats(uint32_t* tokens, uint64_t* elapsed_ms);
}
