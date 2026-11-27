#pragma once

#include <stdint.h>

namespace Process
{
    inline constexpr uint32_t MAX_PROCESSES = 32;
    inline constexpr uint32_t PROCESS_NAME_LENGTH = 32;

    enum class Type : uint8_t
    {
        System,
        Application,
        Command
    };

    enum class State : uint8_t
    {
        Running,
        Exited
    };

    enum class Priority : uint8_t
    {
        Low,
        Normal,
        High
    };

    struct Info
    {
        uint32_t pid;
        Type type;
        State state;
        int exit_code;
        uint32_t open_windows;
        uint64_t start_tick;
        char name[PROCESS_NAME_LENGTH];
        Priority priority;
    };

    void init();
    uint32_t start(const char* name, Type type);
    void set_running(uint32_t pid);
    void finish(uint32_t pid, int exit_code);
    void attach_window(uint32_t pid);
    void detach_window(uint32_t pid);
    uint32_t window_count(uint32_t pid);
    uint32_t snapshot(Info* output, uint32_t capacity);
    uint64_t uptime_ticks();
    bool set_priority(uint32_t pid, Priority priority);
    Priority get_priority(uint32_t pid);
}