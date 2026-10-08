#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Network
{
    struct MACAddress
    {
        uint8_t bytes[6];
    };

    struct IPAddress
    {
        uint8_t bytes[4];
    };

    extern bool available;
    extern MACAddress mac_addr;
    extern IPAddress ip_addr;
    extern IPAddress netmask;
    extern IPAddress gateway;
    extern uint64_t rx_packets;
    extern uint64_t tx_packets;

    void init();
    bool is_available();

    IPAddress get_ip();
    void set_ip(IPAddress ip);
    IPAddress get_netmask();
    void set_netmask(IPAddress mask);
    IPAddress get_gateway();
    void set_gateway(IPAddress gw);
    MACAddress get_mac();

    void poll();
    bool send_packet(const uint8_t* payload, uint16_t length);
    bool ping(IPAddress target_ip, uint32_t timeout_ms = 2000);
    void print_info();
}
