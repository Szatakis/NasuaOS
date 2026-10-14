#pragma once

#include <stdint.h>

namespace Ethernet
{
    struct MACAddress
    {
        uint8_t bytes[6];
    };

    using ReceiveHandler = void (*)(const uint8_t* frame, uint16_t length);

    bool init();
    bool is_available();
    MACAddress get_mac();
    bool send_frame(const uint8_t* frame, uint16_t length);
    void poll(ReceiveHandler receive);
}
