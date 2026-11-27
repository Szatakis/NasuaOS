#include "network_internal.hpp"

#include "drivers/ethernet/driver.hpp"
#include "libs/asm/asm.hpp"

namespace Network
{
    namespace Internal
    {
        namespace
        {
            struct ArpEntry
            {
                IPAddress ip;
                MACAddress mac;
                uint64_t updated;
                bool valid;
            };

            ArpEntry arp_cache[16] = {};

            bool ip_equal(const IPAddress& left, const uint8_t* right)
            {
                for (uint8_t index = 0; index < 4; ++index)
                {
                    if (left.bytes[index] != right[index])
                    {
                        return false;
                    }
                }
                return true;
            }

            void cache_address(const uint8_t* ip, const uint8_t* mac)
            {
                uint8_t target = 0;
                uint64_t oldest_age = 0;
                const uint64_t now = ticks();
                for (uint8_t index = 0; index < 16; ++index)
                {
                    if (arp_cache[index].valid && ip_equal(arp_cache[index].ip, ip))
                    {
                        target = index;
                        oldest_age = UINT64_MAX;
                        break;
                    }
                    const uint64_t age = now - arp_cache[index].updated;
                    if (!arp_cache[index].valid || age > oldest_age)
                    {
                        target = index;
                        oldest_age = age;
                    }
                }
                for (uint8_t index = 0; index < 4; ++index)
                {
                    arp_cache[target].ip.bytes[index] = ip[index];
                }
                for (uint8_t index = 0; index < 6; ++index)
                {
                    arp_cache[target].mac.bytes[index] = mac[index];
                }
                arp_cache[target].updated = now;
                arp_cache[target].valid = true;
            }

            bool cached_address(const IPAddress& ip, MACAddress& mac)
            {
                const uint64_t now = ticks();
                for (uint8_t index = 0; index < 16; ++index)
                {
                    if (arp_cache[index].valid && ip_equal(arp_cache[index].ip, ip.bytes))
                    {
                        if (now - arp_cache[index].updated <= 3000)
                        {
                            mac = arp_cache[index].mac;
                            return true;
                        }
                        arp_cache[index].valid = false;
                    }
                }
                return false;
            }

            bool send_request(const IPAddress& target)
            {
                uint8_t frame[42] = {};
                for (uint8_t index = 0; index < 6; ++index)
                {
                    frame[index] = 0xFF;
                    frame[6 + index] = mac_addr.bytes[index];
                }
                write_be16(frame + 12, 0x0806);
                uint8_t* arp = frame + 14;
                write_be16(arp, 1);
                write_be16(arp + 2, 0x0800);
                arp[4] = 6;
                arp[5] = 4;
                write_be16(arp + 6, 1);
                for (uint8_t index = 0; index < 6; ++index)
                {
                    arp[8 + index] = mac_addr.bytes[index];
                    arp[18 + index] = 0;
                }
                for (uint8_t index = 0; index < 4; ++index)
                {
                    arp[14 + index] = ip_addr.bytes[index];
                    arp[24 + index] = target.bytes[index];
                }
                return send_frame(frame, sizeof(frame));
            }
        }

        void handle_arp(const uint8_t* packet, uint16_t length)
        {
            if (!packet || length < 28 || read_be16(packet) != 1 ||
                read_be16(packet + 2) != 0x0800 || packet[4] != 6 ||
                packet[5] != 4)
            {
                return;
            }

            const uint16_t operation = read_be16(packet + 6);
            if (operation != 1 && operation != 2)
            {
                return;
            }
            cache_address(packet + 14, packet + 8);

            if (operation == 1 && ip_addr.bytes[0] != 0 && ip_equal(ip_addr, packet + 24))
            {
                uint8_t reply[42] = {};
                for (uint8_t index = 0; index < 6; ++index)
                {
                    reply[index] = packet[8 + index];
                    reply[6 + index] = mac_addr.bytes[index];
                }
                write_be16(reply + 12, 0x0806);
                uint8_t* arp = reply + 14;
                write_be16(arp, 1);
                write_be16(arp + 2, 0x0800);
                arp[4] = 6;
                arp[5] = 4;
                write_be16(arp + 6, 2);
                for (uint8_t index = 0; index < 6; ++index)
                {
                    arp[8 + index] = mac_addr.bytes[index];
                    arp[18 + index] = packet[8 + index];
                }
                for (uint8_t index = 0; index < 4; ++index)
                {
                    arp[14 + index] = ip_addr.bytes[index];
                    arp[24 + index] = packet[14 + index];
                }
                send_frame(reply, sizeof(reply));
            }
        }

        bool resolve_arp(const IPAddress& address, MACAddress& mac)
        {
            if (cached_address(address, mac))
            {
                return true;
            }

            for (uint8_t attempt = 0; attempt < 3; ++attempt)
            {
                if (!send_request(address))
                {
                    return false;
                }
                const uint64_t start = ticks();
                while (ticks() - start < 100)
                {
                    Ethernet::poll(handle_frame);
                    if (cached_address(address, mac))
                    {
                        return true;
                    }
                    io_wait();
                }
            }
            return false;
        }
    }
}
