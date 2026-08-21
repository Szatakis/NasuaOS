#include "../driver.hpp"

#include "../controllers/controllers.hpp"

#include "drivers/pci/driver.hpp"

namespace Usb
{

void init()
{
    if (Pci::found_ehci)
    {
        Ehci::init();
    }

    if (Pci::found_ohci)
    {
        Ohci::init();
    }

    if (Pci::found_uhci)
    {
        Uhci::init();
    }

    if (Pci::found_xhci)
    {
        Xhci::init();
    }
}

}