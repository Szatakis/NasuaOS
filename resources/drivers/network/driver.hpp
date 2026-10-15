#pragma once

#include <stdint.h>
#include <stddef.h>

#include "drivers/ethernet/driver.hpp"

namespace Network
{
    using MACAddress = Ethernet::MACAddress;

    struct IPAddress
    {
        uint8_t bytes[4];
    };

    using UDPReceiveHandler = void (*)(const IPAddress& source, uint16_t source_port, const uint8_t* payload, uint16_t length);

    extern bool available;
    extern MACAddress mac_addr;
    extern IPAddress ip_addr;
    extern IPAddress netmask;
    extern IPAddress gateway;
    extern IPAddress dns_server;
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
    IPAddress get_dns_server();
    void set_dns_server(IPAddress dns);
    MACAddress get_mac();
    bool bind_udp_port(uint16_t port, UDPReceiveHandler handler);
    void unbind_udp_port(uint16_t port);
    bool send_udp_packet(IPAddress destination, uint16_t source_port, uint16_t destination_port, const uint8_t* payload, uint16_t length);
    bool resolve_hostname(const char* hostname, IPAddress& address, uint32_t timeout_ms = 3000);

    void poll();
    bool send_packet(const uint8_t* payload, uint16_t length);
    bool ping(IPAddress target_ip, uint32_t timeout_ms = 2000);
    void print_info();
}
