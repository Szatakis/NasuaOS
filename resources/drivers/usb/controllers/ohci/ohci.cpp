#include "ohci.hpp"
#include "../../driver.hpp"

namespace Ohci
{
    void init()
    {
        Uart::puts("[OHCI] Initializing OHCI\n");
    }
}