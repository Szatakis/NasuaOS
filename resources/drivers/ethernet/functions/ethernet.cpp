#include "../driver.hpp"

#include "../controllers/virtio-net/driver.hpp"

namespace Ethernet
{
    namespace
    {
        bool available = false;
        MACAddress mac = {};
    }

    bool init()
    {
        available = false;
        mac = {};
        available = VirtIONet::init(mac);
        return available;
    }

    bool is_available()
    {
        return available && VirtIONet::is_available();
    }

    MACAddress get_mac()
    {
        return mac;
    }

    bool send_frame(const uint8_t* frame, uint16_t length)
    {
        return is_available() && VirtIONet::send(frame, length);
    }

    void poll(ReceiveHandler receive)
    {
        if (is_available())
        {
            VirtIONet::poll(receive);
        }
    }
}
