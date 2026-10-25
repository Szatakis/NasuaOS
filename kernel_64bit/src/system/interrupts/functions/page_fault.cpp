#include "page_fault.hpp"

#include "drivers/memory/driver.hpp"
#include "drivers/uart/driver.hpp"

#include "system/sysfunc/logger/logger.hpp"
#include "kernel/kernel_panic/kernel_panic.hpp"

#include "libs/libc/libc.hpp"

#include "isr.hpp"

extern bool debug_mode;

static uint64_t read_cr2()
{
    uint64_t value;

    asm volatile("mov %%cr2, %0" : "=r"(value));

    return value;
}

// Check whether address looks like PCI/MMIO space

static bool is_mmio_address(uint64_t addr)
{
    /*
        Common PCI MMIO regions.
    
        This is intentionally conservative.
    
        Do NOT allocate normal RAM for these addresses.
    */

    // Legacy PCI/MMIO region commonly used by devices
    if(addr >= 0x80000000ULL && addr <  0x100000000ULL)
    {
        return true;
    }

    // PCI 64-bit MMIO region
    if(addr >= 0x100000000ULL && addr <  0x10000000000ULL)
    {
        return true;
    }

    return false;
}

// Page fault handler

void page_fault_handler(Registers* regs)
{
    uint64_t addr = read_cr2();
    uint64_t error = regs->error;
    uint64_t rip = regs->rip;
    uint64_t rsp = regs->rsp;

    char addr_buf[20];
    char err_buf[20];
    char rip_buf[20];
    char rsp_buf[20];

    to_hex64(addr,  addr_buf);
    to_hex64(error, err_buf);
    to_hex64(rip,   rip_buf);
    to_hex64(rsp,   rsp_buf);

    // Only show detailed page fault info for errors (non-zero error code)
    if (error != 0) {
        Uart::puts("\n[PAGE FAULT] Address: ");
        Uart::puts(addr_buf);
        Uart::puts("\n");

        Uart::puts("[PAGE FAULT] Error: ");
        Uart::puts(err_buf);
        Uart::puts("\n");

        Uart::puts("[PAGE FAULT] RIP: ");
        Uart::puts(rip_buf);
        Uart::puts("\n");

        Uart::puts("[PAGE FAULT] RSP: ");
        Uart::puts(rsp_buf);
        Uart::puts("\n");
    }

    /*
        Do NOT automatically allocate RAM for MMIO.
    
        If we allocated RAM here, the EHCI driver would read
        from normal RAM instead of the USB controller.
    */

    if(is_mmio_address(addr))
    {
        Uart::puts("[PAGE FAULT] Address belongs to MMIO/PCI range\n");

        Uart::puts("[PAGE FAULT] Refusing automatic RAM mapping\n");

        log(ERROR,"PAGE FAULT","Page fault in MMIO/PCI address");

        kernel_panic("Page Fault in MMIO/PCI address", err_buf, rip_buf, rsp_buf, addr_buf);

        return;
    }

    const bool page_exists = (error & 0x1) != 0;
    const bool is_instruction_fetch = (error & 0x10) != 0;

    if (page_exists || is_instruction_fetch)
    {
        Uart::puts(page_exists
            ? "[PAGE FAULT] Protection violation\n"
            : "[PAGE FAULT] Instruction fetch from an unmapped page\n");
        log(ERROR, "PAGE FAULT", page_exists
            ? "Protection violation"
            : "Instruction fetch from an unmapped page");
        kernel_panic(page_exists
            ? "Page Fault - Protection Violation"
            : "Page Fault - Invalid Instruction Fetch",
            err_buf, rip_buf, rsp_buf, addr_buf);
        return;
    }

    // Demand paging is only valid for non-present data accesses.

    uint64_t page = Memory::pmm::alloc_page();

    if(!page)
    {
        Uart::puts("[PAGE FAULT] OUT OF MEMORY\n");

        log(ERROR, "PAGE FAULT", "OUT OF MEMORY");

        kernel_panic("Page Fault - Out of Memory", err_buf, rip_buf, rsp_buf, addr_buf);

        return;
    }

    uint64_t virtual_page = addr & ~0xFFFULL;

    // Only show verbose mapping info if debug mode is enabled
    if (debug_mode)
    {
        Uart::puts("[PAGE FAULT] Mapping normal RAM page\n");

        Uart::puts("[PAGE FAULT] Virtual: ");

        Uart::puthex(virtual_page);

        Uart::puts(" Physical: ");

        Uart::puthex(page);

        Uart::puts("\n");
    }

    Memory::vmm::map_page(virtual_page, page, Memory::vmm::PAGE_WRITE);

    // Only log if debug mode is enabled
    if (debug_mode)
    {
        Uart::puts("[PAGE FAULT] Page allocated\n");
        log(INFO, "PAGE FAULT", "Page allocated");
    }
}