# Kernel Architecture

The NasuaOS kernel is a 64-bit (x86_64) kernel with a modular architecture. The 64-bit kernel is the primary target; a 32-bit kernel (`kernel_32bit`) exists for legacy/compatibility purposes.

## Entry Point

The kernel entry point is `kmain()` in `kernel_64bit/src/kernel/kernel.cpp`. It receives a Multiboot2 structure pointer containing boot information from the Limine bootloader.

### Global State Variables

```cpp
bool debug_mode       = false;     // UART debug output enabled
bool safe_mode        = false;     // Safe mode (restricted initialization)
bool kernel_panicked  = false;     // Set when kernel_panic() is called
```

## Initialization Order (iqu_init)

The `iqu_init()` function initializes all kernel subsystems in a specific order:

```
iqu_init()                // Kernel initialization queue
    ├── uart_init()       // Serial port for debug output
    ├── cpu_init()        // Detect CPU features and cores
    ├── memory_init()     // Initialize physical memory manager
    ├── paging_init()     // Set up page tables and enable paging
    ├── pmm_init()        // Physical page frame allocator
    ├── vmm_init()        // Virtual memory manager
    ├── heap_init()       // Kernel heap (malloc/free)
    ├── interrupt_init()  // IDT setup
    ├── idt_init()        // Interrupt descriptor table
    ├── pit_init()        // Programmable Interval Timer (100 Hz)
    ├── interrupt_enable() // Enable IRQs (sti)
    ├── storage_init()     // ATA disk detection
    ├── pci_init()         // PCI controller enumeration
    ├── usb_init()         // USB controller initialization
    ├── mouse_init()       // PS/2 mouse setup
    ├── image_init()       // Load rootfs/boot modules from Limine
    ├── Process::init()   // Process registry and system service entries
    ├── napp_init()       // Initialize NAPP application loader
    └── shell_init()       // Start the shell
```

### Initialization Functions

| Function        | File          | Purpose                                    |
|-----------------|---------------|--------------------------------------------|
| `uart_init()`   | `serial.cpp`  | Set up COM1 for serial debug output        |
| `cpu_init()`    | `cpu.cpp`     | Detect CPU cores, model, features          |
| `memory_init()` | `memory.cpp`  | Parse Multiboot2 memory map                |
| `paging_init()` | `memory.cpp`  | Set up kernel page tables                  |
| `pmm_init()`    | `pmm.cpp`     | Initialize physical page frame allocator   |
| `vmm_init()`    | `vmm.cpp`     | Initialize virtual memory manager          |
| `heap_init()`   | `heap.cpp`    | Initialize kernel heap (`kmalloc`/`kfree`) |
| `idt_init()`    | `interrupts/` | Set up interrupt descriptor table          |
| `pit_init()`    | `timer.cpp`   | Initialize PIT for timekeeping             |
| `Process::init()` | `system/process/process.cpp` | Initialize process registry and system service entries |

## Subsystems

### Interrupts

| File                                                | Component                         |
|-----------------------------------------------------|-----------------------------------|
| `kernel_64bit/src/system/interrupts/idt.cpp`        | Interrupt Descriptor Table        |
| `kernel_64bit/src/system/interrupts/isr.cpp`        | Interrupt Service Routines        |
| `kernel_64bit/src/system/interrupts/page_fault.cpp` | Page fault handler                |
| `kernel_64bit/src/system/interrupts/pic.cpp`        | Programmable Interrupt Controller |
| `kernel_64bit/src/system/interrupts/madt.cpp`       | Multiple APIC Description Table   |

### Memory Management

| File                              | Component                     |
|-----------------------------------|-------------------------------|
| `kernel_64bit/src/system/memory/` | Memory map parsing and paging |
| `kernel_64bit/src/system/pmm/`    | Physical page frame manager   |
| `kernel_64bit/src/system/vmm/`    | Virtual memory manager        |

### Process Management

| File                                 | Component                 |
|--------------------------------------|---------------------------|
| `kernel_64bit/src/system/process/process.cpp` | Process registry, IDs, lifecycle, and snapshots |
| `kernel_64bit/src/system/process/elf.cpp`     | ELF loading support |

The registry tracks system services, running GUI applications, and completed
NAPP commands for the Task Manager. Applications and shell commands still run
synchronously and are not separate schedulable processes.

### Kernel Scheduler

The x86_64 kernel includes a round-robin scheduler for kernel tasks. PIT ticks
preempt the current task; tasks may also call `Scheduler::yield()` to give up
their time slice. `Scheduler::sleep_ticks()` blocks a task until the requested
number of 10 ms PIT ticks has elapsed; sleeping tasks do not consume round-robin
slots until they wake. `Scheduler::create_task()` allocates a 16 KiB stack and
starts a task entry function with its argument. Returning from the entry
function terminates that task. Tasks share kernel address space and privilege
level; this scheduler does not provide process isolation.

The kernel's boot/main loop is the initial scheduler task. Scheduler tasks can
be exercised with the host-side test:

```bash
g++ -std=c++20 -DSCHEDULER_TEST -Ikernel_64bit/src -Iresources \
  tests/scheduler_test.cpp \
  kernel_64bit/src/system/scheduler/functions/scheduler.cpp \
  kernel_64bit/src/system/scheduler/pr_map/pr_map.cpp \
  -o /tmp/nasua-scheduler-test
/tmp/nasua-scheduler-test
```

### CPU Privilege Rings

The x86_64 GDT defines code/data selectors for rings 0 through 3 and a 64-bit
TSS. The IDT targets the kernel code selector, and the scheduler updates the
TSS kernel-stack pointer when selecting a task with its own stack. The VMM can
create and destroy per-process address spaces with a private lower-half user
mapping while retaining the kernel's upper-half mappings as supervisor pages.
Kernel upper-half PML4 entries are synchronized before activating a user
address space so kernel heap growth remains available to interrupt and syscall
handlers. The scheduler can start ring-3 tasks and switch their address spaces. IDT
vector `0x80` is a DPL-3 syscall gate; its initial ABI supports bounded writes
to the kernel log and task exit, and user exceptions terminate the faulting
task. The syscall wrappers and numbers are in
`utilities/applications/include/nasua_syscall.h`. Exit status is recorded in
the process registry.

NAPP ABI 12 adds an opt-in `user_mode` header flag. The loader maps flagged
images read/execute-only, gives each task a separate writable user stack, and
starts it through the scheduler. Such applications receive no `napp_api`
kernel-function pointers; the currently supported user ABI is limited to
bounded serial writes and exit. `bootcheck` is the first NAPP using this path
and is available from the Start Menu. Older NAPPs and those without the flag
continue using the existing ring-0 ABI. In particular, GUI callbacks,
filesystem services, command arguments, and most applications have not yet
been migrated. Embedded drivers remain in ring 0; x86-64 page permissions do
not distinguish rings 0, 1, and 2 from one another, so those selectors are
descriptor groundwork rather than isolation boundaries.

### System Functions

| Component       | File                        | Description                    |
|-----------------|-----------------------------|--------------------------------|
| Logger          | `sysfunc/logger/`           | 128-entry circular log buffer  |
| Command History | `sysfunc/command_history/`  | Ring buffer (10 entries)       |
| File Resolver   | `filesystem/file_resolver/` | ClawFS/CD overlay              |
| Kernel Panic    | `kernel/kernel_panic/`      | Panic screen with QR code      |
| Info Variables  | `sysfunc/info_vars/`        | Shell path, username, hostname |

## Kernel Panic

The kernel panic handler (`kernel_panic.cpp`) renders a detailed screen on panic:

- Panic reason and error code
- RIP, RSP, and fault address (CR2)
- CPU model string
- QR code linking to debug instructions

```cpp
void kernel_panic(const char* message, const char* error_code, 
                  uint64_t rip, uint64_t rsp, uint64_t fault_addr);
```

## Libc

The kernel provides a minimal C library (`kernel_64bit/src/system/libs/libc/libc.hpp`):

| Function                     | Purpose                 |
|------------------------------|-------------------------|
| `strcpy`, `strncpy`          | String copy             |
| `strcmp`, `strncmp`          | String compare          |
| `strlen`, `strchr`, `strstr` | String search           |
| `strcat`                     | String concatenation    |
| `strrchr`                    | Find last occurrence    |
| `atoi`                       | ASCII to integer        |
| `itoa`                       | Integer to ASCII        |
| `memclear` / `memset`        | Memory fill             |
| `is_empty_or_whitespace`     | Check empty strings     |
| `uint64_to_string`           | Format uint64 as string |

## Assembly Helpers

Low-level operations use inline assembly (`kernel_64bit/src/libs/asm/asm.hpp`):

| Function    | Port            | Purpose                   |
|-------------|-----------------|---------------------------|
| `outb/inb`  | 0x60–0x64, etc. | I/O port byte             |
| `outw/inw`  | Various         | I/O port word             |
| `outl/inl`  | Various         | I/O port long             |
| `hcf()`     | —               | Halt CPU (`hlt; jmp hcf`) |
| `io_wait()` | 0x3F8?          | Brief I/O delay           |

## Kernel Globals

| Variable          | File            | Default      | Description         |
|-------------------|-----------------|--------------|---------------------|
| `debug_mode`      | `kernel.cpp`    | `false`      | UART debug output   |
| `safe_mode`       | `kernel.cpp`    | `false`      | Restricted init     |
| `current_path`    | `info_vars.cpp` | `"/home"`    | Shell CWD           |
| `user_name`       | `info_vars.cpp` | `"user"`     | Username for prompt |
| `pc_name`         | `info_vars.cpp` | `"nasua-pc"` | Hostname            |
| `kernel_panicked` | `kernel.cpp`    | `false`      | Panic flag          |
