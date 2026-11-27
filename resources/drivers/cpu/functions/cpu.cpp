#include "../driver.hpp"
#include <stdint.h>
#include "limine.h"

#include "drivers/uart/driver.hpp"
#include "drivers/apic/driver.hpp"
#include "system/interrupts/functions/idt.hpp"

#include "system/sysfunc/logger/logger.hpp"

extern volatile limine_mp_request mp_request;

static uint64_t detected_core_count = 1;

extern "C" void limine_ap_entry(limine_mp_info* info)
{
    (void)info;
    asm volatile("cli");
    idt_load();
    Apic::init();
    asm volatile("sti");
    for (;;)
    {
        asm volatile("hlt");
    }
}

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx)
{
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf), "c"(subleaf));
}

void Cpu::init_cores() {
    if(mp_request.response)
    {
        detected_core_count = mp_request.response->cpu_count;
        Uart::puts("[CPU] MP available\n");
        log(INFO,"CPU","MP available");
#if defined(__x86_64__)
        for (uint64_t index = 0; index < mp_request.response->cpu_count; ++index)
        {
            limine_mp_info* cpu = mp_request.response->cpus[index];
            if (cpu == nullptr ||
                cpu->lapic_id == mp_request.response->bsp_lapic_id)
            {
                continue;
            }
            cpu->goto_address = nullptr;
        }
#endif
    }
}

void Cpu::start_application_cores()
{
    if (!mp_request.response)
    {
        return;
    }

#if defined(__x86_64__)
    for (uint64_t index = 0; index < mp_request.response->cpu_count; ++index)
    {
        limine_mp_info* cpu = mp_request.response->cpus[index];
        if (cpu == nullptr ||
            cpu->lapic_id == mp_request.response->bsp_lapic_id)
        {
            continue;
        }
        cpu->goto_address = limine_ap_entry;
    }
#endif
}

uint64_t Cpu::core_count()
{
    return detected_core_count;
}

void Cpu::get_brand(char* brand)
{
    uint32_t eax, ebx, ecx, edx;

    char* ptr = brand;

    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++)
    {
        cpuid(leaf, 0, eax, ebx, ecx, edx);

        *(uint32_t*)(ptr + 0)  = eax;
        *(uint32_t*)(ptr + 4)  = ebx;
        *(uint32_t*)(ptr + 8)  = ecx;
        *(uint32_t*)(ptr + 12) = edx;

        ptr += 16;
    }

    brand[48] = '\0';
}

const char* Cpu::get_architecture()
{
    #if defined(__x86_64__)
        return "x86_64";
    #elif defined(__i386__)
        return "x86";
    #elif defined(__aarch64__)
        return "ARM64";
    #elif defined(__arm__)
        return "ARM";
    #else
        return "Unknown";
    #endif
}