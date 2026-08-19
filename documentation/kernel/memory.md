# Kernel - Memory Management

NasuaOS 64-bit kernel implements a layered memory management system: physical frame allocation, virtual memory mapping, and a kernel heap.

## Overview

| Layer | Component | File |
|-------|-----------|------|
| Physical | Physical Memory Manager (PMM) | `system/pmm/` |
| Virtual | Virtual Memory Manager (VMM) | `system/vmm/` |
| Heap | Kernel heap allocator | `system/heap/` |
| Paging | Page table setup | `system/memory/` |

## Physical Memory Manager (PMM)

The PMM manages 4KB physical memory pages using a bitmap.

### Initialization

Called during `iqu_init()` via `pmm_init()`. It parses the Multiboot2 memory map to identify usable RAM regions.

### Functions

| Function | Description |
|----------|-------------|
| `pmm_init(uint64_t mem_map_addr, uint64_t mem_map_length)` | Initialize with Multiboot2 memory map |
| `pmm_alloc_page()` | Allocate a single 4KB page |
| `pmm_alloc_pages(int count)` | Allocate multiple contiguous pages |
| `pmm_free_page(uint64_t addr)` | Free a previously allocated page |
| `pmm_get_free_memory()` | Return total free memory in bytes |
| `pmm_get_used_memory()` | Return total used memory in bytes |
| `pmm_get_total_memory()` | Return total system memory in bytes |

### Bitmap

The PMM uses a bitmap where each bit represents one 4KB page:

```cpp
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];
```

- Bit set (1) = page in use
- Bit clear (0) = page available

### Memory Map Parsing

The kernel parses the Multiboot2 memory map provided by the Limine bootloader:

```cpp
struct multiboot_mmap_entry
{
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
    uint32_t reserved;
};
```

Only entries of type `1` (available RAM) are used for allocation.

## Virtual Memory Manager (VMM)

The VMM manages virtual-to-physical page mapping.

### Functions

| Function | Description |
|----------|-------------|
| `vmm_init()` | Initialize virtual memory manager |
| `vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags)` | Map a virtual address to a physical address |
| `vmm_unmap_page(uint64_t vaddr)` | Remove a page mapping |
| `vmm_get_physical_address(uint64_t vaddr)` | Translate virtual to physical |
| `vmm_init_kernel_address_space()` | Set up kernel address space (typically 0xFFFFFFFF80000000) |

### Page Table Flags

```cpp
#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_WRITABLE  (1ULL << 1)
#define PAGE_USER      (1ULL << 2)
#define PAGE_NX        (1ULL << 63)
```

### Address Space Layout

| Range | Type | Purpose |
|-------|------|---------|
| `0x0000000000000000` – `0x00000000FFFFFFFF` | User space | NAPP applications |
| `0xFFFFFFFF80000000` – `0xFFFFFFFFFFFFFFFF` | Kernel space | Kernel code and data |

NAPP applications run in user space (ring 3). The kernel runs in supervisor mode (ring 0).

## Kernel Heap

The heap provides `kmalloc`/`kfree` dynamically allocation, similar to `malloc`/`free`.

### Functions

| Function | Description |
|----------|-------------|
| `kmalloc(size_t size)` | Allocate `size` bytes, returns pointer |
| `kfree(void* ptr)` | Free a previously allocated block |
| `krealloc(void* ptr, size_t size)` | Resize an allocation |
| `heap_init()` | Initialize the heap |

### Implementation

The heap uses a simple block-list allocator:

```cpp
struct heap_block
{
    size_t size;
    bool is_free;
    heap_block* next;
    heap_block* prev;
};
```

- Blocks are 8-byte aligned
- Minimum allocation size: 16 bytes
- Uses boundary tag for coalescing on free

### Heap Size

The kernel heap is allocated from PMM pages and grows as needed:

```cpp
#define KERNEL_HEAP_SIZE (16 * 1024 * 1024)  // 16 MB
```

## Paging Initialization

Called via `paging_init()` during `iqu_init()`:

1. Creates page table hierarchy (PML4 → PDPT → PD → PT)
2. Identity-maps the first 1MB of memory
3. Maps the kernel to `0xFFFFFFFF80000000`
4. Maps the backbuffer and heap
5. Loads CR3, enables the NX bit, and loads the new page tables

### Page Fault Handler

Source: `kernel_64bit/src/system/interrupts/page_fault.cpp`

```cpp
void page_fault_handler(interrupt_frame_t* frame)
{
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    // ... handle fault
    kernel_panic("Page fault", ...);
}
```

## Memory Information

The `info` command displays current memory usage:

```
Hardware information
  CPU:             Intel(R) Core(TM) ...
  Total RAM:       2048MB
  Used RAM:        128MB
```

## Memory Alignment

| Component | Alignment |
|-----------|-----------|
| Pages | 4KB (0x1000) |
| Heap blocks | 8-byte |
| Stack | 16-byte |
| NAPP binaries | No alignment requirement |

## Memory Protection

- NAPP applications run with the `USER` page flag set
- Kernel pages are supervisor-mode only
- The NX (No Execute) bit is set on data pages to prevent code injection
