#include "page_fault.hpp"

#include "system/drivers/memory/driver.hpp"
#include "system/drivers/uart/driver.hpp"

#include "kernel/include/logger/logger.hpp"

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
     * Common PCI MMIO regions.
     *
     * This is intentionally conservative.
     *
     * Do NOT allocate normal RAM for these addresses.
     */

    // Legacy PCI/MMIO region commonly used by devices
    if(addr >= 0x80000000ULL &&
       addr <  0x100000000ULL)
    {
        return true;
    }

    // PCI 64-bit MMIO region
    if(addr >= 0x100000000ULL &&
       addr <  0x10000000000ULL)
    {
        return true;
    }

    return false;
}

// Page fault handler

void page_fault_handler(uint64_t error)
{
    uint64_t addr = read_cr2();

    Uart::puts("\n[PAGE FAULT]\n");

    Uart::puts("[PAGE FAULT] Address: ");
    Uart::puthex(addr);
    Uart::puts("\n");

    Uart::puts("[PAGE FAULT] Error: ");
    Uart::puthex(error);
    Uart::puts("\n");

    /*
     * Do NOT automatically allocate RAM for MMIO.
     *
     * Example:
     *
     * EHCI BAR = 0x80860000
     *
     * If we allocated RAM here, the EHCI driver would read
     * from normal RAM instead of the USB controller.
     */

    if(is_mmio_address(addr))
    {
        Uart::puts(
            "[PAGE FAULT] Address belongs to MMIO/PCI range\n"
        );

        Uart::puts(
            "[PAGE FAULT] Refusing automatic RAM mapping\n"
        );

        log(
            ERROR,
            "PAGE FAULT",
            "Page fault in MMIO/PCI address"
        );

        while(1)
        {
            asm volatile("cli; hlt");
        }
    }

    /*
     * Normal demand paging.
     */

    uint64_t page =
        pmm_alloc_page();

    if(!page)
    {
        Uart::puts(
            "[PAGE FAULT] OUT OF MEMORY\n"
        );

        log(
            ERROR,
            "PAGE FAULT",
            "OUT OF MEMORY"
        );

        while(1)
        {
            asm volatile("cli; hlt");
        }
    }

    uint64_t virtual_page =
        addr & ~0xFFFULL;

    Uart::puts(
        "[PAGE FAULT] Mapping normal RAM page\n"
    );

    Uart::puts(
        "[PAGE FAULT] Virtual: "
    );

    Uart::puthex(virtual_page);

    Uart::puts(
        " Physical: "
    );

    Uart::puthex(page);

    Uart::puts("\n");

    vmm_map_page(
        virtual_page,
        page,
        PAGE_WRITE
    );

    Uart::puts(
        "[PAGE FAULT] Page allocated\n"
    );

    log(
        INFO,
        "PAGE FAULT",
        "Page allocated"
    );
}