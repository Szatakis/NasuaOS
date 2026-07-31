#ifndef MEMORY_H
#define MEMORY_H

#include <limine.h>
#include <stdint.h>
#include <cstdint>
#include <cstddef>

void memory_init();
uint64_t memory_used();
const char* memory_total();
uint64_t memory_total_bytes();

extern "C" {
    void *memcpy(void *__restrict dest, const void *__restrict src, std::size_t n);
    void *memset(void *s, int c, std::size_t n);
    void *memmove(void *dest, const void *src, std::size_t n);
    int memcmp(const void *s1, const void *s2, std::size_t n);
}

//heap
void heap_init();
void* kmalloc(size_t size);
void kfree(void* ptr);

//paging
void paging_init();
uint64_t get_cr3();
uint64_t get_hhdm_offset();

//pmm
#define PAGE_SIZE 4096

void pmm_init();
uint64_t pmm_alloc_page();
void pmm_free_page(uint64_t addr);
uint64_t pmm_get_total_memory();

//vmm
#define PAGE_PRESENT 0x001
#define PAGE_WRITE   0x002
#define PAGE_USER    0x004
#define PAGE_NX      (1ULL << 63)

void vmm_init();
bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
bool vmm_unmap_page(uint64_t virt);
uint64_t vmm_translate(uint64_t virt);

#endif