#include "../driver.hpp"

namespace Memory 
{
    namespace vmm 
    {
        static constexpr uint64_t PAGE_ADDRESS_MASK = 0x000FFFFFFFFFF000ULL;
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

        static uint64_t read_active_cr3()
        {
            uint64_t cr3;
            asm volatile("mov %%cr3, %0" : "=r"(cr3));
            return cr3 & ~0xFFFULL;
        }

        static void write_cr3(uint64_t cr3)
        {
            asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
        }

        static uint64_t* table_from_entry(uint64_t entry)
        {
            return phys_to_virt(entry & ~0xFFFULL);
        }

        static uint64_t* get_user_leaf_table(const UserAddressSpace* space, uint64_t virt, bool create)
        {
            if (space == nullptr || !space->valid || virt < space->base ||
                virt >= (1ULL << 47) ||
                ((virt >> 39) & 0x1FF) != space->pml4_index)
            {
                return nullptr;
            }

            uint64_t* root = phys_to_virt(space->root_phys);
            uint64_t& root_entry = root[space->pml4_index];
            if (!(root_entry & PAGE_PRESENT))
            {
                if (!create)
                {
                    return nullptr;
                }

                uint64_t* pdpt = create_table();
                if (pdpt == nullptr)
                {
                    return nullptr;
                }
                root_entry = (reinterpret_cast<uint64_t>(pdpt) - Memory::paging::get_hhdm_offset()) |
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            }

            uint64_t* pdpt = table_from_entry(root_entry);
            uint64_t& pdpt_entry = pdpt[(virt >> 30) & 0x1FF];
            if (!(pdpt_entry & PAGE_PRESENT))
            {
                if (!create)
                {
                    return nullptr;
                }

                uint64_t* pd = create_table();
                if (pd == nullptr)
                {
                    return nullptr;
                }
                pdpt_entry = (reinterpret_cast<uint64_t>(pd) - Memory::paging::get_hhdm_offset()) |
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            }
            else
            {
                if (!(pdpt_entry & PAGE_USER))
                {
                    if (!create)
                    {
                        return nullptr;
                    }
                    pdpt_entry |= PAGE_USER;
                }
            }

            uint64_t* pd = table_from_entry(pdpt_entry);
            uint64_t& pd_entry = pd[(virt >> 21) & 0x1FF];
            if (!(pd_entry & PAGE_PRESENT))
            {
                if (!create)
                {
                    return nullptr;
                }

                uint64_t* pt = create_table();
                if (pt == nullptr)
                {
                    return nullptr;
                }
                pd_entry = (reinterpret_cast<uint64_t>(pt) - Memory::paging::get_hhdm_offset()) |
                    PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            }
            else
            {
                if (!(pd_entry & PAGE_USER))
                {
                    if (!create)
                    {
                        return nullptr;
                    }
                    pd_entry |= PAGE_USER;
                }
            }

            return table_from_entry(pd_entry);
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
            constexpr uint64_t LARGE_PAGE_1G_MASK = 0x000FFFFFC0000000ULL;
            constexpr uint64_t LARGE_PAGE_2M_MASK = 0x000FFFFFFFE00000ULL;

            if(!(pml4[pml4_i] & PAGE_PRESENT))
            {
                return 0;
            }

            uint64_t* pdpt = phys_to_virt(pml4[pml4_i] & ~0xFFF);
            if(!(pdpt[pdpt_i] & PAGE_PRESENT))
            {
                return 0;
            }

            if(pdpt[pdpt_i] & (1ULL << 7))
            {
                return ((pdpt[pdpt_i] & LARGE_PAGE_1G_MASK) | (virt & 0x3FFFFFFFULL)) & PAGE_ADDRESS_MASK;
            }

            uint64_t* pd = phys_to_virt(pdpt[pdpt_i] & ~0xFFF);
            if(!(pd[pd_i] & PAGE_PRESENT))
            {
                return 0;
            }

            if(pd[pd_i] & (1ULL << 7))
            {
                return ((pd[pd_i] & LARGE_PAGE_2M_MASK) | (virt & 0x1FFFFFULL)) & PAGE_ADDRESS_MASK;
            }

            uint64_t* pt = phys_to_virt(pd[pd_i] & ~0xFFF);
            if(!(pt[pt_i] & PAGE_PRESENT))
            {
                return 0;
            }

            return pt[pt_i] & PAGE_ADDRESS_MASK;
        }

        bool create_user_address_space(UserAddressSpace* space)
        {
            if (space == nullptr || pml4 == nullptr)
            {
                return false;
            }

            *space = {};
            const uint64_t root_phys = Memory::pmm::alloc_page();
            if (root_phys == 0)
            {
                return false;
            }

            uint64_t* root = phys_to_virt(root_phys);
            for (uint32_t index = 0; index < 512; ++index)
            {
                root[index] = index >= 256 ? pml4[index] & ~PAGE_USER : 0;
            }

            constexpr uint16_t USER_PML4_FIRST = 1;
            constexpr uint16_t USER_PML4_END = 256;
            uint16_t user_index = USER_PML4_FIRST;
            while (user_index < USER_PML4_END && (pml4[user_index] & PAGE_PRESENT))
            {
                ++user_index;
            }

            if (user_index == USER_PML4_END)
            {
                Memory::pmm::free_page(root_phys);
                return false;
            }

            space->root_phys = root_phys;
            space->base = static_cast<uint64_t>(user_index) << 39;
            space->pml4_index = user_index;
            space->valid = true;
            return true;
        }

        bool map_user_page(UserAddressSpace* space, uint64_t virt, uint64_t phys, uint64_t flags)
        {
            if ((virt & 0xFFF) != 0 || (phys & 0xFFF) != 0 ||
                (phys & ~PAGE_ADDRESS_MASK) != 0 || phys == 0 ||
                !(flags & PAGE_USER))
            {
                return false;
            }

            uint64_t* pt = get_user_leaf_table(space, virt, true);
            if (pt == nullptr)
            {
                return false;
            }

            uint64_t& entry = pt[(virt >> 12) & 0x1FF];
            if (entry & PAGE_PRESENT)
            {
                return false;
            }

            entry = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
            if (read_active_cr3() == space->root_phys)
            {
                asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
            }
            return true;
        }

        bool unmap_user_page(UserAddressSpace* space, uint64_t virt)
        {
            if ((virt & 0xFFF) != 0)
            {
                return false;
            }

            uint64_t* pt = get_user_leaf_table(space, virt, false);
            if (pt == nullptr)
            {
                return false;
            }

            uint64_t& entry = pt[(virt >> 12) & 0x1FF];
            if (!(entry & PAGE_PRESENT))
            {
                return false;
            }

            const uint64_t physical_page = entry & PAGE_ADDRESS_MASK;
            entry = 0;
            Memory::pmm::free_page(physical_page);
            if (read_active_cr3() == space->root_phys)
            {
                asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
            }
            return true;
        }

        bool is_user_range(const UserAddressSpace* space, uint64_t virt, uint64_t length,
                           bool write, bool execute)
        {
            if (space == nullptr || !space->valid || length == 0 ||
                virt < space->base || virt >= (1ULL << 47) ||
                length > (1ULL << 47) - virt ||
                ((virt >> 39) & 0x1FF) != space->pml4_index)
            {
                return false;
            }

            const uint64_t last_page = (virt + length - 1) & ~0xFFFULL;
            for (uint64_t page = virt & ~0xFFFULL;; page += 0x1000)
            {
                uint64_t* pt = get_user_leaf_table(space, page, false);
                if (pt == nullptr)
                {
                    return false;
                }

                const uint64_t entry = pt[(page >> 12) & 0x1FF];
                if (!(entry & PAGE_PRESENT) || !(entry & PAGE_USER) ||
                    (write && !(entry & PAGE_WRITE)) ||
                    (execute && (entry & PAGE_NX)))
                {
                    return false;
                }

                if (page == last_page)
                {
                    return true;
                }
            }
        }

        bool activate_address_space(UserAddressSpace* space)
        {
            if (space == nullptr || !space->valid || space->root_phys == 0)
            {
                return false;
            }

            if (!sync_kernel_mappings(space))
            {
                return false;
            }
            write_cr3(space->root_phys);
            return true;
        }

        bool sync_kernel_mappings(UserAddressSpace* space)
        {
            if (space == nullptr || !space->valid || space->root_phys == 0 || pml4 == nullptr)
            {
                return false;
            }

            uint64_t* root = phys_to_virt(space->root_phys);
            for (uint32_t index = 256; index < 512; ++index)
            {
                root[index] = pml4[index] & ~PAGE_USER;
            }
            return true;
        }

        void activate_kernel_address_space()
        {
            write_cr3(Memory::paging::get_cr3() & ~0xFFFULL);
        }

        bool destroy_user_address_space(UserAddressSpace* space)
        {
            if (space == nullptr || !space->valid || space->root_phys == 0 ||
                read_active_cr3() == space->root_phys)
            {
                return false;
            }

            uint64_t* root = phys_to_virt(space->root_phys);
            uint64_t& root_entry = root[space->pml4_index];
            if (root_entry & PAGE_PRESENT)
            {
                uint64_t pdpt_phys = root_entry & ~0xFFFULL;
                uint64_t* pdpt = table_from_entry(root_entry);
                for (uint32_t pdpt_index = 0; pdpt_index < 512; ++pdpt_index)
                {
                    const uint64_t pdpt_entry = pdpt[pdpt_index];
                    if (!(pdpt_entry & PAGE_PRESENT))
                    {
                        continue;
                    }

                    uint64_t* pd = table_from_entry(pdpt_entry);
                    for (uint32_t pd_index = 0; pd_index < 512; ++pd_index)
                    {
                        const uint64_t pd_entry = pd[pd_index];
                        if (!(pd_entry & PAGE_PRESENT))
                        {
                            continue;
                        }

                        uint64_t* pt = table_from_entry(pd_entry);
                        for (uint32_t pt_index = 0; pt_index < 512; ++pt_index)
                        {
                            const uint64_t page_entry = pt[pt_index];
                            if (page_entry & PAGE_PRESENT)
                            {
                                Memory::pmm::free_page(page_entry & PAGE_ADDRESS_MASK);
                            }
                        }

                        Memory::pmm::free_page(pd_entry & ~0xFFFULL);
                    }

                    Memory::pmm::free_page(pdpt_entry & ~0xFFFULL);
                }

                Memory::pmm::free_page(pdpt_phys);
            }

            Memory::pmm::free_page(space->root_phys);
            *space = {};
            return true;
        }
    }
}