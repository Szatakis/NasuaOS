#include "page_fault.hpp"

#include "system/drivers/memory/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "system/sysfunc/logger/logger.hpp"
#include "kernel/kernel_panic/kernel_panic.hpp"

#include "libs/libc/libc.hpp"

#include "isr.hpp"

static uint64_t read_cr2()
{
    uint64_t value;

    asm volatile(
        "mov %%cr2, %0"
        : "=r"(value)
    );

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

        kernel_panic(
            "Page Fault in MMIO/PCI address",
            err_buf,
            rip_buf,
            rsp_buf,
            addr_buf
        );

        return;
    }

    /*
        Normal demand paging.
    */

    uint64_t page = pmm_alloc_page();

    if(!page)
    {
        Uart::puts("[PAGE FAULT] OUT OF MEMORY\n");

        log(ERROR, "PAGE FAULT", "OUT OF MEMORY");

        kernel_panic(
            "Page Fault - Out of Memory",
            err_buf,
            rip_buf,
            rsp_buf,
            addr_buf
        );

        return;
    }

    bool page_exists = (error & 0x1) != 0;  
    bool is_write    = (error & 0x2) != 0;

    if (page_exists && is_write) {
        Uart::puts("[PAGE FAULT] Write violation on read-only page!\n");
        log(ERROR, "PAGE FAULT", "Attempted write to read-only memory");
    
        kernel_panic(
            "Access Violation - Write to Read-Only Memory",
            err_buf,
            rip_buf,
            rsp_buf,
            addr_buf
        );
        return;
    }

    uint64_t virtual_page = addr & ~0xFFFULL;

    Uart::puts("[PAGE FAULT] Mapping normal RAM page\n");

    Uart::puts("[PAGE FAULT] Virtual: ");

    Uart::puthex(virtual_page);

    Uart::puts(" Physical: ");

    Uart::puthex(page);

    Uart::puts("\n");

    vmm_map_page(virtual_page, page, PAGE_WRITE);

    Uart::puts("[PAGE FAULT] Page allocated\n");

    log(INFO, "PAGE FAULT", "Page allocated");
}