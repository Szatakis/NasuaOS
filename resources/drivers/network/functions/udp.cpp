#include "network_internal.hpp"

namespace Network
{
    namespace
    {
        struct UDPBinding
        {
            uint16_t port;
            UDPReceiveHandler handler;
        };

        UDPBinding bindings[8] = {};
    }

    bool bind_udp_port(uint16_t port, UDPReceiveHandler handler)
    {
        if (port == 0 || !handler)
        {
            return false;
        }
        for (UDPBinding& binding : bindings)
        {
            if (binding.port == port)
            {
                binding.handler = handler;
                return true;
            }
        }
        for (UDPBinding& binding : bindings)
        {
            if (binding.port == 0)
            {
                binding.port = port;
                binding.handler = handler;
                return true;
            }
        }
        return false;
    }

    void unbind_udp_port(uint16_t port)
    {
        for (UDPBinding& binding : bindings)
        {
            if (binding.port == port)
            {
                binding.port = 0;
                binding.handler = nullptr;
                return;
            }
        }
    }

    bool send_udp_packet(IPAddress destination, uint16_t source_port, uint16_t destination_port, const uint8_t* payload, uint16_t length)
    {
        return Internal::send_udp(ip_addr, destination, source_port, destination_port, payload, length);
    }

    namespace Internal
    {
        bool send_udp(const IPAddress& source, const IPAddress& destination, uint16_t source_port, uint16_t destination_port, const uint8_t* payload, uint16_t length)
        {
            if ((!payload && length != 0) || length > 1472 || source_port == 0 || destination_port == 0)
            {
                return false;
            }

            uint8_t packet[8 + 1472] = {};
            write_be16(packet, source_port);
            write_be16(packet + 2, destination_port);
            write_be16(packet + 4, static_cast<uint16_t>(8 + length));
            for (uint16_t index = 0; index < length; ++index)
            {
                packet[8 + index] = payload[index];
            }

            uint8_t checksum_packet[12 + 8 + 1472] = {};
            for (uint8_t index = 0; index < 4; ++index)
            {
                checksum_packet[index] = source.bytes[index];
                checksum_packet[4 + index] = destination.bytes[index];
            }
            checksum_packet[9] = 17;
            write_be16(checksum_packet + 10, static_cast<uint16_t>(8 + length));
            for (uint16_t index = 0; index < 8 + length; ++index)
            {
                checksum_packet[12 + index] = packet[index];
            }
            uint16_t udp_checksum = checksum(checksum_packet, 12 + 8 + length);
            if (udp_checksum == 0)
            {
                udp_checksum = 0xFFFF;
            }
            write_be16(packet + 6, udp_checksum);
            return send_ipv4(source, destination, 17, packet, static_cast<uint16_t>(8 + length));
        }

        void handle_udp(const IPAddress& source, const IPAddress& destination, const uint8_t* packet, uint16_t length)
        {
            if (!packet || length < 8)
            {
                return;
            }
            const uint16_t udp_length = read_be16(packet + 4);
            if (udp_length < 8 || udp_length > length)
            {
                return;
            }
            const uint16_t udp_checksum = read_be16(packet + 6);
            if (udp_checksum != 0)
            {
                uint8_t checksum_packet[12 + 1480] = {};
                for (uint8_t index = 0; index < 4; ++index)
                {
                    checksum_packet[index] = source.bytes[index];
                    checksum_packet[4 + index] = destination.bytes[index];
                }
                checksum_packet[9] = 17;
                write_be16(checksum_packet + 10, udp_length);
                for (uint16_t index = 0; index < udp_length; ++index)
                {
                    checksum_packet[12 + index] = packet[index];
                }
                if (checksum(checksum_packet, 12 + udp_length) != 0)
                {
                    return;
                }
            }

            const uint16_t source_port = read_be16(packet);
            const uint16_t destination_port = read_be16(packet + 2);
            const uint8_t* payload = packet + 8;
            const uint16_t payload_length = static_cast<uint16_t>(udp_length - 8);

            if (source_port == 67 && destination_port == 68)
            {
                dhcp_receive(payload, payload_length);
            }
            for (const UDPBinding& binding : bindings)
            {
                if (binding.port == destination_port && binding.handler)
                {
                    binding.handler(source, source_port, payload, payload_length);
                }
            }
        }
    }
}
