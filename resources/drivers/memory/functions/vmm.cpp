#include "../driver.hpp"

namespace Memory 
{
    namespace vmm 
    {
        static uint64_t* pml4 = nullptr;

        static uint64_t* phys_to_virt(uint64_t phys)
        {
            return (uint64_t*)(phys + Memory::paging::get_hhdm_offset());
        }

        static uint64_t* create_table()
        {
            uint64_t page = Memory::pmm::alloc_page();
            if(!page)
            {
                return nullptr;
            }

            uint64_t* table = phys_to_virt(page);

            for(int i = 0; i < 512; i++)
            {
                table[i] = 0;
            }

            return table;
        }

        void init()
        {
            Uart::puts("[VMM] Initializing...\n");
            log(INFO, "VMM", "Initializing...");

            uint64_t cr3 = Memory::paging::get_cr3();

            Uart::puts("[VMM] CR3: ");
            Uart::puthex(cr3);
            Uart::puts("\n");

            pml4 = phys_to_virt(cr3 & ~0xFFF);

            Uart::puts("[VMM] Initialized\n");
            log(INFO, "VMM", "Initialized");
        }

        bool map_page(uint64_t virt, uint64_t phys, uint64_t flags)
        {
            uint64_t pml4_i = (virt >> 39) & 0x1FF;
            uint64_t pdpt_i = (virt >> 30) & 0x1FF;
            uint64_t pd_i   = (virt >> 21) & 0x1FF;
            uint64_t pt_i   = (virt >> 12) & 0x1FF;

            uint64_t* pdpt;

            if(!(pml4[pml4_i] & PAGE_PRESENT))
            {
                pdpt = create_table();
                if(!pdpt)
                {
                    return false;
                }

                pml4[pml4_i] = ((uint64_t)pdpt - Memory::paging::get_hhdm_offset()) | PAGE_PRESENT | PAGE_WRITE;
            }
            else
            {
                pdpt = phys_to_virt(pml4[pml4_i] & ~0xFFF);
            }

            uint64_t* pd;

            if(!(pdpt[pdpt_i] & PAGE_PRESENT))
            {
                pd = create_table();
                if(!pd)
                {
                    return false;
                }

                pdpt[pdpt_i] = ((uint64_t)pd - Memory::paging::get_hhdm_offset()) | PAGE_PRESENT | Memory::vmm::PAGE_WRITE;
            }
            else
            {
                pd = phys_to_virt(pdpt[pdpt_i] & ~0xFFF);
            }

            uint64_t* pt;

            if(!(pd[pd_i] & PAGE_PRESENT))
            {
                pt = create_table();
                if(!pt)
                {
                    return false;
                }

                pd[pd_i] = ((uint64_t)pt - Memory::paging::get_hhdm_offset()) | PAGE_PRESENT | PAGE_WRITE;
            }
            else
            {
                pt = phys_to_virt(pd[pd_i] & ~0xFFF);
            }

            pt[pt_i] = (phys & ~0xFFF) | flags | PAGE_PRESENT;

            // Clear NX bit → allow execution (needed for flat binaries)
            pt[pt_i] &= ~(1ULL << 63);

            asm volatile("invlpg (%0)" : : "r"(virt) : "memory");

            return true;
        }

        bool unmap_page(uint64_t virt)
        {
            uint64_t pml4_i = (virt >> 39) & 0x1FF;
            uint64_t pdpt_i = (virt >> 30) & 0x1FF;
            uint64_t pd_i   = (virt >> 21) & 0x1FF;
            uint64_t pt_i   = (virt >> 12) & 0x1FF;

            if(!(pml4[pml4_i] & PAGE_PRESENT))
            {
                return false;
            }

            uint64_t* pdpt = phys_to_virt(pml4[pml4_i] & ~0xFFF);
            if(!(pdpt[pdpt_i] & PAGE_PRESENT))
            {
                return false;
            }

            uint64_t* pd = phys_to_virt(pdpt[pdpt_i] & ~0xFFF);
            if(!(pd[pd_i] & PAGE_PRESENT))
            {
                return false;
            }

            uint64_t* pt = phys_to_virt(pd[pd_i] & ~0xFFF);
            if(!(pt[pt_i] & PAGE_PRESENT))
            {
                return false;
            }

            pt[pt_i] = 0;

            asm volatile("invlpg (%0)" : : "r"(virt) : "memory");

            return true;
        }

        uint64_t translate(uint64_t virt)
        {
            uint64_t pml4_i = (virt >> 39) & 0x1FF;
            uint64_t pdpt_i = (virt >> 30) & 0x1FF;
            uint64_t pd_i   = (virt >> 21) & 0x1FF;
            uint64_t pt_i   = (virt >> 12) & 0x1FF;

            if(!(pml4[pml4_i] & PAGE_PRESENT))
            {
                return 0;
            }

            uint64_t* pdpt = phys_to_virt(pml4[pml4_i] & ~0xFFF);
            if(!(pdpt[pdpt_i] & PAGE_PRESENT))
            {
                return 0;
            }

            uint64_t* pd = phys_to_virt(pdpt[pdpt_i] & ~0xFFF);
            if(!(pd[pd_i] & PAGE_PRESENT))
            {
                return 0;
            }

            uint64_t* pt = phys_to_virt(pd[pd_i] & ~0xFFF);
            if(!(pt[pt_i] & PAGE_PRESENT))
            {
                return 0;
            }

            return pt[pt_i] & ~0xFFF;
        }
    }
}