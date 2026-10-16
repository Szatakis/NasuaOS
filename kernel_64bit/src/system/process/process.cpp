#include "process.hpp"

#include "drivers/timer/driver.hpp"
#include "system/sysfunc/logger/logger.hpp"

namespace Process
{
    static Info processes[MAX_PROCESSES];
    static uint32_t next_pid = 1;
    static bool initialized = false;

    static void copy_name(char* destination, const char* source)
    {
        uint32_t index = 0;
        if (source != nullptr)
        {
            while (source[index] != '\0' && index < PROCESS_NAME_LENGTH - 1)
            {
                destination[index] = source[index];
                ++index;
            }
        }
        destination[index] = '\0';
    }

    static Info* find(uint32_t pid)
    {
        if (pid == 0)
        {
            return nullptr;
        }
        for (uint32_t index = 0; index < MAX_PROCESSES; ++index)
        {
            if (processes[index].pid == pid)
            {
                return &processes[index];
            }
        }
        return nullptr;
    }

    uint32_t start(const char* name, Type type)
    {
        if (!initialized || name == nullptr || name[0] == '\0')
        {
            log(ERROR, "PROCESS", "Cannot start process with invalid name or before initialization");
            return 0;
        }

        Info* slot = nullptr;
        for (uint32_t index = 0; index < MAX_PROCESSES; ++index)
        {
            if (processes[index].pid == 0)
            {
                slot = &processes[index];
                break;
            }
        }
        if (slot == nullptr)
        {
            for (uint32_t index = 0; index < MAX_PROCESSES; ++index)
            {
                if (processes[index].state == State::Exited &&
                    (slot == nullptr || processes[index].pid < slot->pid))
                {
                    slot = &processes[index];
                }
            }
        }
        if (slot == nullptr)
        {
            log(ERROR, "PROCESS", "Process table is full; no exited process can be reclaimed");
            return 0;
        }

        slot->pid = next_pid++;
        if (next_pid == 0)
        {
            next_pid = 1;
        }
        slot->type = type;
        slot->state = State::Running;
        slot->exit_code = 0;
        slot->open_windows = 0;
        slot->start_tick = Timer::pit_get_ticks();
        copy_name(slot->name, name);
        return slot->pid;
    }

    void init()
    {
        for (uint32_t index = 0; index < MAX_PROCESSES; ++index)
        {
            processes[index] = {};
        }
        next_pid = 1;
        initialized = true;

        start("kernel", Type::System);
        start("shell", Type::System);
        start("memory manager", Type::System);
        start("storage", Type::System);
        start("filesystem", Type::System);
        start("network", Type::System);
        start("graphics", Type::System);
        start("input", Type::System);
        start("timer", Type::System);
        start("NAPP runtime", Type::System);
    }

    void set_running(uint32_t pid)
    {
        Info* process = find(pid);
        if (process != nullptr)
        {
            process->state = State::Running;
            process->exit_code = 0;
        }
    }

    void finish(uint32_t pid, int exit_code)
    {
        Info* process = find(pid);
        if (process != nullptr)
        {
            process->state = State::Exited;
            process->exit_code = exit_code;
            process->open_windows = 0;
        }
    }

    void attach_window(uint32_t pid)
    {
        Info* process = find(pid);
        if (process != nullptr)
        {
            ++process->open_windows;
            set_running(pid);
        }
    }

    void detach_window(uint32_t pid)
    {
        Info* process = find(pid);
        if (process == nullptr || process->open_windows == 0)
        {
            return;
        }

        --process->open_windows;
        if (process->open_windows == 0 && process->type == Type::Application)
        {
            finish(pid, 0);
        }
    }

    uint32_t window_count(uint32_t pid)
    {
        const Info* process = find(pid);
        return process != nullptr ? process->open_windows : 0;
    }

    uint32_t snapshot(Info* output, uint32_t capacity)
    {
        if (output == nullptr || capacity == 0)
        {
            return 0;
        }

        uint32_t copied = 0;
        for (uint32_t index = 0; index < MAX_PROCESSES && copied < capacity; ++index)
        {
            if (processes[index].pid != 0)
            {
                output[copied++] = processes[index];
            }
        }
        return copied;
    }

    uint64_t uptime_ticks()
    {
        return Timer::pit_get_ticks();
    }
}