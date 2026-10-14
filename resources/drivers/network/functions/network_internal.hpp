#pragma once

#include "../driver.hpp"

namespace Network
{
    namespace Internal
    {
        uint16_t read_be16(const uint8_t* bytes);
        void write_be16(uint8_t* bytes, uint16_t value);
        uint32_t read_be32(const uint8_t* bytes);
        void write_be32(uint8_t* bytes, uint32_t value);
        uint16_t checksum(const uint8_t* data, uint32_t length);
        uint64_t ticks();
        bool send_frame(const uint8_t* frame, uint16_t length);
        void handle_frame(const uint8_t* frame, uint16_t length);
        void handle_arp(const uint8_t* packet, uint16_t length);
        void handle_ipv4(const uint8_t* packet, uint16_t length);
        void handle_udp(const IPAddress& source, const IPAddress& destination, const uint8_t* packet, uint16_t length);
        bool send_udp(const IPAddress& source, const IPAddress& destination, uint16_t source_port, uint16_t destination_port, const uint8_t* payload, uint16_t length);
        bool send_ipv4(const IPAddress& source, const IPAddress& destination, uint8_t protocol, const uint8_t* payload, uint16_t length);
        bool resolve_arp(const IPAddress& address, MACAddress& mac);
        void dhcp_start();
        void dhcp_poll();
        void dhcp_receive(const uint8_t* payload, uint16_t length);
    }
}
