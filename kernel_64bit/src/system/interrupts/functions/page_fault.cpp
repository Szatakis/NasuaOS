#include "page_fault.hpp"

#include "system/drivers/memory/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "kernel/include/logger/logger.hpp"
#include "kernel/include/panic/kernel_panic.hpp"

#include "libs/libc/libc.h"

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

void page_fault_handler(uint64_t error)
{
    uint64_t addr = read_cr2();

    Uart::puts("\n[PAGE FAULT] Address: ");
    Uart::puthex(addr);
    Uart::puts("\n");

    Uart::puts("[PAGE FAULT] Error: ");
    Uart::puthex(error);
    Uart::puts("\n");

    char addr_buf[20];
    char err_buf[20];

    to_hex64(addr,  addr_buf);
    to_hex64(error, err_buf);

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
            "Unknown",
            "Unknown",
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
            "Unknown",
            "Unknown",
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
            "Unknown", "Unknown", addr_buf
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