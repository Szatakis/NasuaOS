#include "../driver.hpp"
#include "../controllers/controllers.hpp"

#include "system/drivers/pci/driver.hpp"

void usb_init()
{
    if(found_ehci)
    {
        ehci_init();
    }
    if(found_ohci)
    {
        ohci_init();
    }
    if(found_uhci)
    {
        uhci_init();
    }
    if(found_xhci)
    {
        xhci_init();
    }
}