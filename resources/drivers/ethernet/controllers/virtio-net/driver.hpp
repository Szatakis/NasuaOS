#pragma once

#include <stdint.h>

namespace Ethernet
{
    struct MACAddress;
    using ReceiveHandler = void (*)(const uint8_t* frame, uint16_t length);
}

namespace VirtIONet
{
    bool init(Ethernet::MACAddress& mac);
    bool is_available();
    bool send(const uint8_t* frame, uint16_t length);
    void poll(Ethernet::ReceiveHandler receive);
}
