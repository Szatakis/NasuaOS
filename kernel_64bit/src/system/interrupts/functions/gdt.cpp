#include "gdt.hpp"

#include "drivers/uart/driver.hpp"
#include "system/sysfunc/logger/logger.hpp"

namespace Gdt
{
    struct TaskStateSegment
    {
        uint32_t reserved0;
        uint64_t rsp0;
        uint64_t rsp1;
        uint64_t rsp2;
        uint64_t reserved1;
        uint64_t ist1;
        uint64_t ist2;
        uint64_t ist3;
        uint64_t ist4;
        uint64_t ist5;
        uint64_t ist6;
        uint64_t ist7;
        uint64_t reserved2;
        uint16_t reserved3;
        uint16_t io_map_base;
    } __attribute__((packed));

    struct DescriptorTablePointer
    {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    static uint64_t gdt[11] __attribute__((aligned(16)));
    static TaskStateSegment tss;
    extern "C" void gdt_load(const DescriptorTablePointer* pointer);

    void init()
    {
        gdt[0] = 0;
        gdt[1] = 0x00AF9A000000FFFFULL;
        gdt[2] = 0x00CF92000000FFFFULL;
        gdt[3] = 0x00AFBA000000FFFFULL;
        gdt[4] = 0x00CFB2000000FFFFULL;
        gdt[5] = 0x00AFDA000000FFFFULL;
        gdt[6] = 0x00CFD2000000FFFFULL;
        gdt[7] = 0x00CFF2000000FFFFULL;
        gdt[8] = 0x00AFFA000000FFFFULL;

        tss = {};
        tss.io_map_base = sizeof(TaskStateSegment);
        const uint64_t tss_base = reinterpret_cast<uint64_t>(&tss);
        const uint64_t tss_limit = sizeof(TaskStateSegment) - 1;
        gdt[9] = (tss_limit & 0xFFFF) |
                 ((tss_base & 0xFFFFFF) << 16) |
                 (0x89ULL << 40) |
                 ((tss_limit & 0xF0000) << 32) |
                 ((tss_base & 0xFF000000) << 32);
        gdt[10] = tss_base >> 32;

        const DescriptorTablePointer pointer =
        {
            static_cast<uint16_t>(sizeof(gdt) - 1),
            reinterpret_cast<uint64_t>(&gdt)
        };
        gdt_load(&pointer);

        Uart::puts("[GDT] Ring 0-3 descriptors and TSS loaded\n");
        log(INFO, "GDT", "Ring 0-3 descriptors and TSS loaded");
    }

    void set_kernel_stack(uint64_t stack_top)
    {
        tss.rsp0 = stack_top;
    }
}
