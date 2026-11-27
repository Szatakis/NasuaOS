#include "../driver.hpp"

extern "C" 
{
    extern volatile limine_hhdm_request hhdm_request;
}

namespace Memory 
{
    namespace paging 
    {
        static uint64_t current_cr3 = 0;
        static uint64_t current_hhdm = 0;

        static uint64_t read_cr3()
        {
            uint64_t value;
            asm volatile("mov %%cr3, %0" : "=r"(value));
            return value;
        }

        uint64_t get_cr3()
        {
            return current_cr3;
        }

        uint64_t get_hhdm_offset()
        {
            return current_hhdm;
        }

        void init()
        {
            Uart::puts("[PAGING] Initializing...\n");
            log(INFO, "PAGING", "Initializing...");

            current_cr3 = read_cr3();

            Uart::puts("[PAGING] Current CR3: ");
            Uart::puthex(current_cr3);
            Uart::puts("\n");

            if(!::hhdm_request.response)
            {
                Uart::puts("[PAGING] ERROR: HHDM missing\n");
                log(ERROR, "PAGING", "HHDM missing");
                return;
            }

            current_hhdm = ::hhdm_request.response->offset;

            Uart::puts("[PAGING] HHDM: ");
            Uart::puthex(current_hhdm);
            Uart::puts("\n");

            Uart::puts("[PAGING] Enabled\n");
            log(INFO, "PAGING", "Enabled");
        }
    }
}