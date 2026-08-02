#include "page_fault.hpp"

#include "system/drivers/memory/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "kernel/include/logger/logger.hpp"

static uint64_t read_cr2()
{
    uint64_t value;

    asm volatile(
        "mov %%cr2,%0"
        :
        "=r"(value)
    );

    return value;
}

void page_fault_handler(uint64_t error)
{
    (void)error;

    uint64_t addr = read_cr2();

    Uart::puts("[PAGE FAULT] Address: ");
    Uart::puthex(addr);
    Uart::puts("\n");

    uint64_t page = pmm_alloc_page();

    if(!page)
    {
        Uart::puts("[PAGE FAULT] OUT OF MEMORY\n");
        log(ERROR,"PAGE FAULT","OUT OF MEMORY");

        while(1)
        {
            asm volatile("cli;hlt");
        }
    }

    vmm_map_page(
        addr & ~0xFFF,
        page,
        PAGE_WRITE
    );

    Uart::puts("[PAGE FAULT] Page allocated\n");
    log(INFO,"PAGE FAULT","Page allocated");
}