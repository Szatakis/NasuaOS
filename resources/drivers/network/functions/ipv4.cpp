#include "network_internal.hpp"

#include "drivers/ethernet/driver.hpp"
#include "libs/asm/asm.hpp"

namespace Network
{
    namespace Internal
    {
        namespace
        {
            uint16_t next_ip_id = 1;
            uint16_t ping_sequence = 0;
            bool ping_reply_received = false;
            uint16_t ping_reply_identifier = 0x4E53;
            uint16_t ping_reply_sequence = 0;
            IPAddress ping_target = {};
            bool ping_in_progress = false;

            bool is_broadcast(const IPAddress& address)
            {
                return address.bytes[0] == 255 && address.bytes[1] == 255 && address.bytes[2] == 255 && address.bytes[3] == 255;
            }

            bool is_local_destination(const IPAddress& address)
            {
                if (address.bytes[0] == 0 && address.bytes[1] == 0 && address.bytes[2] == 0 && address.bytes[3] == 0)
                {
                    return ip_addr.bytes[0] == 0;
                }
                if (is_broadcast(address))
                {
                    return true;
                }
                for (uint8_t index = 0; index < 4; ++index)
                {
                    if (address.bytes[index] != ip_addr.bytes[index])
                    {
                        return false;
                    }
                }
                return true;
            }

            bool same_subnet(const IPAddress& left, const IPAddress& right)
            {
                for (uint8_t index = 0; index < 4; ++index)
                {
                    if ((left.bytes[index] & netmask.bytes[index]) != (right.bytes[index] & netmask.bytes[index]))
                    {
                        return false;
                    }
                }
                return true;
            }

            bool send_ipv4_packet(const IPAddress& source, const IPAddress& destination, uint8_t protocol, const uint8_t* payload, uint16_t length)
            {
                if (!payload || length > 1480)
                {
                    return false;
                }

                MACAddress destination_mac = {};
                if (is_broadcast(destination))
                {
                    for (uint8_t index = 0; index < 6; ++index)
                    {
                        destination_mac.bytes[index] = 0xFF;
                    }
                }
                else
                {
                    IPAddress next_hop = destination;
                    if (!same_subnet(source, destination))
                    {
                        if (gateway.bytes[0] == 0)
                        {
                            return false;
                        }
                        next_hop = gateway;
                    }
                    if (!resolve_arp(next_hop, destination_mac))
                    {
                        return false;
                    }
                }

                uint8_t frame[14 + 20 + 1480] = {};
                for (uint8_t index = 0; index < 6; ++index)
                {
                    frame[index] = destination_mac.bytes[index];
                    frame[6 + index] = mac_addr.bytes[index];
                }
                write_be16(frame + 12, 0x0800);

                uint8_t* ip = frame + 14;
                ip[0] = 0x45;
                write_be16(ip + 2, static_cast<uint16_t>(20 + length));
                write_be16(ip + 4, next_ip_id++);
                write_be16(ip + 6, 0x4000);
                ip[8] = 64;
                ip[9] = protocol;
                for (uint8_t index = 0; index < 4; ++index)
                {
                    ip[12 + index] = source.bytes[index];
                    ip[16 + index] = destination.bytes[index];
                }
                write_be16(ip + 10, checksum(ip, 20));
                for (uint16_t index = 0; index < length; ++index)
                {
                    ip[20 + index] = payload[index];
                }
                return send_frame(frame, static_cast<uint16_t>(14 + 20 + length));
            }
        }

        uint16_t read_be16(const uint8_t* bytes)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
        }

        void write_be16(uint8_t* bytes, uint16_t value)
        {
            bytes[0] = static_cast<uint8_t>(value >> 8);
            bytes[1] = static_cast<uint8_t>(value);
        }

        uint32_t read_be32(const uint8_t* bytes)
        {
            return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) | (static_cast<uint32_t>(bytes[2]) << 8) | static_cast<uint32_t>(bytes[3]);
        }

        void write_be32(uint8_t* bytes, uint32_t value)
        {
            bytes[0] = static_cast<uint8_t>(value >> 24);
            bytes[1] = static_cast<uint8_t>(value >> 16);
            bytes[2] = static_cast<uint8_t>(value >> 8);
            bytes[3] = static_cast<uint8_t>(value);
        }

        uint16_t checksum(const uint8_t* data, uint32_t length)
        {
            uint32_t sum = 0;
            while (length > 1)
            {
                sum += (static_cast<uint16_t>(data[0]) << 8) | data[1];
                data += 2;
                length -= 2;
            }
            if (length)
            {
                sum += static_cast<uint16_t>(data[0]) << 8;
            }
            while (sum >> 16)
            {
                sum = (sum & 0xFFFF) + (sum >> 16);
            }
            return static_cast<uint16_t>(~sum);
        }

        bool send_ipv4(const IPAddress& source, const IPAddress& destination, uint8_t protocol, const uint8_t* payload, uint16_t length)
        {
            return send_ipv4_packet(source, destination, protocol, payload, length);
        }

        void handle_ipv4(const uint8_t* packet, uint16_t length)
        {
            if (!packet || length < 20 || (packet[0] >> 4) != 4)
            {
                return;
            }
            const uint16_t header_length = static_cast<uint16_t>((packet[0] & 0x0F) * 4);
            const uint16_t total_length = read_be16(packet + 2);
            if (header_length < 20 || header_length > length || total_length < header_length || total_length > length || checksum(packet, header_length) != 0)
            {
                return;
            }

            const uint16_t fragment = read_be16(packet + 6);
            if (fragment & 0x3FFF)
            {
                return;
            }

            IPAddress source = {};
            IPAddress destination = {};
            for (uint8_t index = 0; index < 4; ++index)
            {
                source.bytes[index] = packet[12 + index];
                destination.bytes[index] = packet[16 + index];
            }
            const uint8_t* payload = packet + header_length;
            const uint16_t payload_length = static_cast<uint16_t>(total_length - header_length);
            const bool pending_dhcp_reply = ip_addr.bytes[0] == 0 && packet[9] == 17 && payload_length >= 4 && read_be16(payload + 2) == 68;
            if (!is_local_destination(destination) && !pending_dhcp_reply)
            {
                return;
            }

            if (packet[9] == 17)
            {
                handle_udp(source, destination, payload, payload_length);
            }
            else if (packet[9] == 1 && payload_length >= 8 && checksum(payload, payload_length) == 0)
            {
                const uint16_t identifier = read_be16(payload + 4);
                const uint16_t sequence = read_be16(payload + 6);
                if (payload[0] == 8 && ip_addr.bytes[0] != 0)
                {
                    uint8_t reply[1480] = {};
                    for (uint16_t index = 0; index < payload_length; ++index)
                    {
                        reply[index] = payload[index];
                    }
                    reply[0] = 0;
                    write_be16(reply + 2, 0);
                    write_be16(reply + 2, checksum(reply, payload_length));
                    send_ipv4(ip_addr, source, 1, reply, payload_length);
                }
                else if (ping_in_progress && payload[0] == 0 && payload[1] == 0 && identifier == ping_reply_identifier && sequence == ping_reply_sequence && source.bytes[0] == ping_target.bytes[0] && source.bytes[1] == ping_target.bytes[1] && source.bytes[2] == ping_target.bytes[2] && source.bytes[3] == ping_target.bytes[3])
                {
                    ping_reply_received = true;
                }
            }
        }

        bool ping(const IPAddress& target, uint32_t timeout_ms)
        {
            if (!available || ip_addr.bytes[0] == 0)
            {
                return false;
            }
            const bool target_is_zero = target.bytes[0] == 0 && target.bytes[1] == 0 && target.bytes[2] == 0 && target.bytes[3] == 0;
            const bool target_is_broadcast = target.bytes[0] == 255 && target.bytes[1] == 255 && target.bytes[2] == 255 && target.bytes[3] == 255;
            if (target_is_zero || target_is_broadcast || target.bytes[0] >= 224)
            {
                return false;
            }
            uint8_t packet[40] = {};
            packet[0] = 8;
            write_be16(packet + 4, ping_reply_identifier);
            const uint16_t sequence = ++ping_sequence;
            write_be16(packet + 6, sequence);
            for (uint8_t index = 0; index < 32; ++index)
            {
                packet[8 + index] = static_cast<uint8_t>('a' + index % 26);
            }
            write_be16(packet + 2, checksum(packet, sizeof(packet)));

            ping_reply_received = false;
            ping_reply_sequence = sequence;
            ping_target = target;
            ping_in_progress = true;
            if (!send_ipv4(ip_addr, target, 1, packet, sizeof(packet)))
            {
                ping_in_progress = false;
                return false;
            }

            const uint64_t timeout_ticks = (timeout_ms + 9) / 10;
            const uint64_t start = ticks();
            while (ticks() - start < timeout_ticks)
            {
                Ethernet::poll(handle_frame);
                dhcp_poll();
                if (ping_reply_received)
                {
                    ping_in_progress = false;
                    return true;
                }
                io_wait();
            }
            ping_in_progress = false;
            return false;
        }
    }

    bool ping(IPAddress target_ip, uint32_t timeout_ms)
    {
        return Internal::ping(target_ip, timeout_ms);
    }
}
