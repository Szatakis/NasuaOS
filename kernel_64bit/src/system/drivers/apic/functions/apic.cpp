#include "../driver.hpp"
#include <limine.h>

#include "system/drivers/memory/driver.hpp"
#include "system/drivers/uart/driver.hpp"
#include "system/interrupts/interrupts.hpp"

#include "system/sysfunc/logger/logger.hpp"

#include "libs/asm/asm.hpp"


#define IA32_APIC_BASE_MSR 0x1B

// Default LAPIC  addres (compatibile with IA32_APIC_BASE MSR in most of the systems) 
// used only as fallback if MSR returns 0.
#define LAPIC_BASE_PHYS_DEFAULT 0xFEE00000ULL

// Registers LAPIC (offsets of the base)
#define LAPIC_REG_ID        0x20
#define LAPIC_REG_TPR       0x80
#define LAPIC_REG_EOI       0xB0
#define LAPIC_REG_SVR       0xF0

#define LAPIC_SVR_ENABLE    0x100

// Registers IOAPIC
#define IOAPIC_REG_IOREGSEL 0x00
#define IOAPIC_REG_IOWIN    0x10
#define IOAPIC_REG_REDTBL   0x10

#define IOAPIC_REDTBL_MASKED 0x10000

extern volatile limine_hhdm_request hhdm_request;

static bool g_apic_active = false;
static bool g_lapic_mapped = false;
static bool g_ioapic_mapped = false;

static uint64_t g_ioapic_base_phys = 0;
static uint32_t g_ioapic_pin_irq0 = 0;


// Helpers: HHDM / MMIO

static inline uint64_t hhdm_offset()
{
    if(!hhdm_request.response)
    {
        return 0;
    }

    return hhdm_request.response->offset;
}

static void map_mmio_once(bool& already_mapped, uint64_t phys_base)
{
    if(already_mapped)
    {
        return;
    }

    uint64_t phys_page = phys_base & ~0xFFFULL;
    uint64_t virt_page = hhdm_offset() + phys_page;

    vmm_map_page(virt_page, phys_page, PAGE_WRITE);

    already_mapped = true;
}

static inline volatile uint32_t* lapic_reg(uint32_t offset)
{
    uint64_t base = apic_read_base() & 0xFFFFF000ULL;

    if(!base)
    {
        base = LAPIC_BASE_PHYS_DEFAULT;
    }

    map_mmio_once(g_lapic_mapped, base);

    return (volatile uint32_t*)(uintptr_t)(hhdm_offset() + base + offset);
}

static inline volatile uint32_t* ioapic_regsel()
{
    map_mmio_once(g_ioapic_mapped, g_ioapic_base_phys);

    return (volatile uint32_t*)(uintptr_t)(hhdm_offset() + g_ioapic_base_phys + IOAPIC_REG_IOREGSEL);
}

static inline volatile uint32_t* ioapic_iowin()
{
    map_mmio_once(g_ioapic_mapped, g_ioapic_base_phys);

    return (volatile uint32_t*)(uintptr_t)(hhdm_offset() + g_ioapic_base_phys + IOAPIC_REG_IOWIN);
}

[[maybe_unused]] static uint32_t ioapic_read(uint8_t reg)
{
    *ioapic_regsel() = reg;
    return *ioapic_iowin();
}

static void ioapic_write(uint8_t reg, uint32_t value)
{
    *ioapic_regsel() = reg;
    *ioapic_iowin() = value;
}


// CPUID

bool apic_available()
{
    uint32_t eax, ebx, ecx, edx;

    asm volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
        : "cc"
    );

    // CPUID.1:EDX bit 9 = APIC on chip
    return (edx & (1u << 9)) != 0;
}


// Local APIC: base MSR

uint64_t apic_read_base()
{
    uint32_t eax;
    uint32_t edx;

    asm volatile(
        "mov %2, %%ecx\n"
        "rdmsr"
        : "=a"(eax), "=d"(edx)
        : "r"(IA32_APIC_BASE_MSR)
        : "ecx"
    );

    return ((uint64_t)edx << 32) | eax;
}

static void apic_write_base(uint64_t value)
{
    uint32_t eax = value & 0xFFFFFFFF;
    uint32_t edx = value >> 32;

    asm volatile(
        "mov %0, %%eax\n"
        "mov %1, %%edx\n"
        "mov $0x1B, %%ecx\n"
        "wrmsr"
        :
        : "r"(eax),
          "r"(edx)
        : "eax",
          "edx",
          "ecx"
    );
}

bool apic_enabled()
{
    uint64_t base = apic_read_base();

    return (base & (1ULL << 11)) != 0;
}

void apic_enable()
{
    Uart::puts("[APIC] Enabling...\n");
    log(INFO,"APIC","Enabling...");

    uint64_t base = apic_read_base();

    if(base & (1ULL << 11))
    {
        Uart::puts("[APIC] Already enabled\n");
        log(INFO,"APIC","Already enabled");
        return;
    }

    base |= (1ULL << 11);

    apic_write_base(base);

    Uart::puts("[APIC] Enabled\n");
    log(INFO,"APIC","Enabled");
}

void apic_disable()
{
    Uart::puts("[APIC] Disabling...\n");
    log(INFO,"APIC","Disabling...");

    uint64_t base = apic_read_base();

    if(!(base & (1ULL << 11)))
    {
        Uart::puts("[APIC] Already disabled\n");
        log(INFO,"APIC","Already disabled");
        return;
    }

    base &= ~(1ULL << 11);

    apic_write_base(base);

    Uart::puts("[APIC] Disabled\n");
    log(INFO,"APIC","Disabled");
}


// Local APIC: init / EOI

void lapic_init()
{
    Uart::puts("[LAPIC] Initializing...\n");
    log(INFO,"LAPIC","Initializing...");

    apic_enable();

    *lapic_reg(LAPIC_REG_TPR) = 0;

    *lapic_reg(LAPIC_REG_SVR) = 0xFF | LAPIC_SVR_ENABLE;

    Uart::puts("[LAPIC] Ready\n");
    log(INFO,"LAPIC","Ready");
}

void lapic_send_eoi()
{
    *lapic_reg(LAPIC_REG_EOI) = 0;
}

static uint32_t lapic_get_id()
{
    return (*lapic_reg(LAPIC_REG_ID)) >> 24;
}


// I/O APIC

void ioapic_mask_irq(uint8_t irq)
{
    uint8_t low_reg = IOAPIC_REG_REDTBL + irq * 2;

    ioapic_write(low_reg, IOAPIC_REDTBL_MASKED);
    ioapic_write(low_reg + 1, 0);
}

void ioapic_set_irq(uint8_t irq, uint8_t vector, uint32_t dest_apic_id)
{
    uint8_t low_reg = IOAPIC_REG_REDTBL + irq * 2;
    uint8_t high_reg = low_reg + 1;

    // fixed delivery, physical dest mode, active-high, edge-triggered, unmasked
    uint32_t low = vector;
    uint32_t high = dest_apic_id << 24;

    ioapic_write(high_reg, high);
    ioapic_write(low_reg, low);
}

void ioapic_init()
{
    Uart::puts("[IOAPIC] Initializing...\n");
    log(INFO,"IOAPIC","Initializing...");

    // mask all 24 lines IRQ on start
    for(uint8_t irq = 0; irq < 24; irq++)
    {
        ioapic_mask_irq(irq);
    }

    // IRQ0 (PIT) -> vector 32, on pin choosed by MADT
    ioapic_set_irq((uint8_t)g_ioapic_pin_irq0, 32, lapic_get_id());

    Uart::puts("[IOAPIC] IRQ0 -> vector 32 (pin: ");
    Uart::puthex(g_ioapic_pin_irq0);
    Uart::puts(")\n");
    log(INFO,"IOAPIC","IRQ0 -> vector 32");

    Uart::puts("[IOAPIC] Ready\n");
    log(INFO,"IOAPIC","Ready");
}


// Choose interrupts controller

bool apic_is_active()
{
    return g_apic_active;
}

void interrupts_controller_init()
{
    MadtInfo madt = madt_parse();

    if(apic_available() && madt.valid && madt.has_ioapic)
    {
        Uart::puts("[IRQ] APIC + IOAPIC available (MADT), using LAPIC + IOAPIC\n");
        log(INFO,"IRQ","APIC + IOAPIC available (MADT), using LAPIC + IOAPIC");

        g_ioapic_base_phys = madt.ioapic_base;
        g_ioapic_pin_irq0 = madt.irq0_gsi - madt.ioapic_gsi_base;

        pic_disable();

        outb(0x22, 0x70);
        outb(0x23, 0x01);

        lapic_init();
        ioapic_init();

        g_apic_active = true;
    }
    else
    {
        Uart::puts("[IRQ] No usable IOAPIC (MADT), using legacy 8259 PIC\n");
        log(INFO,"IRQ","No usable IOAPIC (MADT), using legacy 8259 PIC");

        apic_disable();
        pic_remap();

        g_apic_active = false;
    }
}
