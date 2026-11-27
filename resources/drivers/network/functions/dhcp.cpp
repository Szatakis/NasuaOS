#include "network_internal.hpp"

#if defined(__x86_64__)
#include "drivers/uart/driver.hpp"
#endif

namespace Network
{
    namespace Internal
    {
        namespace
        {
            enum class DHCPState : uint8_t
            {
                Stopped,
                Discovering,
                Requesting,
                Bound,
                Failed
            };

            struct DHCPOptions
            {
                uint8_t message_type;
                IPAddress server;
                IPAddress mask;
                IPAddress router;
                IPAddress dns;
                bool has_message_type;
                bool has_server;
                bool has_mask;
                bool has_router;
                bool has_dns;
            };

            DHCPState state = DHCPState::Stopped;
            uint32_t transaction_id = 0;
            uint8_t attempts = 0;
            uint64_t retry_at = 0;
            IPAddress offered_address = {};
            IPAddress selected_server = {};
            IPAddress offered_mask = {};
            IPAddress offered_router = {};
            IPAddress offered_dns = {};

            void log_dhcp(const char* message)
            {
#if defined(__x86_64__)
                Uart::puts("[DHCP] ");
                Uart::puts(message);
                Uart::puts("\n");
#else
                (void)message;
#endif
            }

            void log_address(const char* label, const IPAddress& address)
            {
#if defined(__x86_64__)
                Uart::puts("[DHCP] ");
                Uart::puts(label);
                for (uint8_t index = 0; index < 4; ++index)
                {
                    Uart::putdec(address.bytes[index]);
                    if (index != 3)
                    {
                        Uart::putc('.');
                    }
                }
                Uart::putc('\n');
#else
                (void)label;
                (void)address;
#endif
            }

            bool address_is_zero(const IPAddress& address)
            {
                return address.bytes[0] == 0 && address.bytes[1] == 0 &&
                       address.bytes[2] == 0 && address.bytes[3] == 0;
            }

            bool address_is_usable(const IPAddress& address)
            {
                return !address_is_zero(address) && address.bytes[0] < 224 && !(address.bytes[0] == 127);
            }

            void append_option(uint8_t* packet, uint16_t& cursor, uint8_t code,
                               const uint8_t* value, uint8_t length)
            {
                packet[cursor++] = code;
                packet[cursor++] = length;
                for (uint8_t index = 0; index < length; ++index)
                {
                    packet[cursor++] = value[index];
                }
            }

            uint16_t create_transaction_id()
            {
                const uint64_t now = ticks();
                uint32_t id = static_cast<uint32_t>(now ^ (now >> 32));
                for (uint8_t index = 0; index < 6; ++index)
                {
                    id = (id << 5) | (id >> 27);
                    id ^= mac_addr.bytes[index];
                }
                return static_cast<uint16_t>(id ^ (id >> 16));
            }

            bool transmit_message(bool request)
            {
                uint8_t packet[300] = {};
                packet[0] = 1;
                packet[1] = 1;
                packet[2] = 6;
                write_be32(packet + 4, transaction_id);
                const uint64_t elapsed = ticks();
                write_be16(packet + 8, elapsed > 6553500 ? 0xFFFF : static_cast<uint16_t>(elapsed / 100));
                write_be16(packet + 10, 0x8000);
                for (uint8_t index = 0; index < 6; ++index)
                {
                    packet[28 + index] = mac_addr.bytes[index];
                }
                packet[236] = 99;
                packet[237] = 130;
                packet[238] = 83;
                packet[239] = 99;

                uint16_t cursor = 240;
                const uint8_t type = request ? 3 : 1;
                append_option(packet, cursor, 53, &type, 1);
                const uint8_t client_identifier[7] = {
                    1, mac_addr.bytes[0], mac_addr.bytes[1], mac_addr.bytes[2],
                    mac_addr.bytes[3], mac_addr.bytes[4], mac_addr.bytes[5]
                };
                append_option(packet, cursor, 61, client_identifier, sizeof(client_identifier));

                if (request)
                {
                    append_option(packet, cursor, 50, offered_address.bytes, 4);
                    append_option(packet, cursor, 54, selected_server.bytes, 4);
                }

                const uint8_t parameter_request_list[] = {1, 3, 6, 15, 51, 58, 59};
                append_option(packet, cursor, 55, parameter_request_list, sizeof(parameter_request_list));
                packet[cursor++] = 255;

                IPAddress source = {};
                IPAddress destination = {{255, 255, 255, 255}};
                if (!send_udp(source, destination, 68, 67, packet, cursor))
                {
                    log_dhcp("Unable to queue a DHCP packet for transmission");
                    return false;
                }
                return true;
            }

            void begin_discovery()
            {
                transaction_id = (static_cast<uint32_t>(create_transaction_id()) << 16) | static_cast<uint32_t>(create_transaction_id() ^ 0xA5C3);
                if (transaction_id == 0)
                {
                    transaction_id = 1;
                }
                offered_address = {};
                selected_server = {};
                offered_mask = {};
                offered_router = {};
                offered_dns = {};
                attempts = 0;
                state = DHCPState::Discovering;
                log_dhcp("state DISCOVERING: sending DHCPDISCOVER");
                transmit_message(false);
                attempts = 1;
                retry_at = ticks() + 100;
            }

            bool parse_options(const uint8_t* packet, uint16_t length, DHCPOptions& options)
            {
                if (length < 240 || packet[236] != 99 || packet[237] != 130 || packet[238] != 83 || packet[239] != 99)
                {
                    return false;
                }
                options = {};
                uint16_t cursor = 240;
                bool end_seen = false;
                while (cursor < length)
                {
                    const uint8_t code = packet[cursor++];
                    if (code == 0)
                    {
                        continue;
                    }
                    if (code == 255)
                    {
                        end_seen = true;
                        break;
                    }
                    if (cursor >= length)
                    {
                        return false;
                    }
                    const uint8_t option_length = packet[cursor++];
                    if (option_length > length - cursor)
                    {
                        return false;
                    }
                    const uint8_t* value = packet + cursor;
                    if (code == 53 && option_length == 1)
                    {
                        options.message_type = value[0];
                        options.has_message_type = true;
                    }
                    else if (code == 54 && option_length == 4)
                    {
                        for (uint8_t index = 0; index < 4; ++index)
                        {
                            options.server.bytes[index] = value[index];
                        }
                        options.has_server = true;
                    }
                    else if (code == 1 && option_length == 4)
                    {
                        for (uint8_t index = 0; index < 4; ++index)
                        {
                            options.mask.bytes[index] = value[index];
                        }
                        options.has_mask = true;
                    }
                    else if (code == 3 && option_length >= 4)
                    {
                        for (uint8_t index = 0; index < 4; ++index)
                        {
                            options.router.bytes[index] = value[index];
                        }
                        options.has_router = true;
                    }
                    else if (code == 6 && option_length >= 4)
                    {
                        for (uint8_t index = 0; index < 4; ++index)
                        {
                            options.dns.bytes[index] = value[index];
                        }
                        options.has_dns = true;
                    }
                    cursor = static_cast<uint16_t>(cursor + option_length);
                }
                return end_seen && options.has_message_type;
            }

            bool same_address(const IPAddress& left, const IPAddress& right)
            {
                for (uint8_t index = 0; index < 4; ++index)
                {
                    if (left.bytes[index] != right.bytes[index])
                    {
                        return false;
                    }
                }
                return true;
            }

            void apply_configuration(const uint8_t* packet, const DHCPOptions& options)
            {
                IPAddress assigned = {};
                for (uint8_t index = 0; index < 4; ++index)
                {
                    assigned.bytes[index] = packet[16 + index];
                }
                if (address_is_zero(assigned))
                {
                    assigned = offered_address;
                }

                ip_addr = assigned;
                netmask = options.has_mask ? options.mask : offered_mask;
                gateway = options.has_router ? options.router : offered_router;
                dns_server = options.has_dns ? options.dns : offered_dns;
                state = DHCPState::Bound;
                log_dhcp("state BOUND: DHCPACK applied IPv4, mask, gateway, and DNS configuration");
                log_address("IPv4 ", ip_addr);
                log_address("Subnet mask ", netmask);
                log_address("Gateway ", gateway);
                log_address("DNS server ", dns_server);
            }
        }

        void dhcp_start()
        {
            if (!available)
            {
                state = DHCPState::Stopped;
                return;
            }
            begin_discovery();
        }

        void dhcp_poll()
        {
            if (state != DHCPState::Discovering && state != DHCPState::Requesting)
            {
                return;
            }
            if (ticks() < retry_at)
            {
                return;
            }
            if (attempts >= 4)
            {
                state = DHCPState::Failed;
                log_dhcp("no DHCP server response after retransmissions; address remains unconfigured");
                return;
            }

            const bool request = state == DHCPState::Requesting;
            log_dhcp(request ? "retransmitting DHCPREQUEST" : "retransmitting DHCPDISCOVER");
            transmit_message(request);
            ++attempts;
            const uint64_t delay = static_cast<uint64_t>(100) << (attempts - 1);
            retry_at = ticks() + (delay > 800 ? 800 : delay);
        }

        void dhcp_receive(const uint8_t* packet, uint16_t length)
        {
            if (!packet || length < 240 || packet[0] != 2 || packet[1] != 1 || packet[2] != 6 || read_be32(packet + 4) != transaction_id)
            {
                return;
            }
            for (uint8_t index = 0; index < 6; ++index)
            {
                if (packet[28 + index] != mac_addr.bytes[index])
                {
                    return;
                }
            }

            DHCPOptions options = {};
            if (!parse_options(packet, length, options))
            {
                return;
            }

            if (state == DHCPState::Discovering && options.message_type == 2 &&
                options.has_server)
            {
                IPAddress offered = {};
                for (uint8_t index = 0; index < 4; ++index)
                {
                    offered.bytes[index] = packet[16 + index];
                }
                if (!address_is_usable(offered) || !address_is_usable(options.server))
                {
                    return;
                }

                offered_address = offered;
                selected_server = options.server;
                offered_mask = options.mask;
                offered_router = options.router;
                offered_dns = options.dns;
                attempts = 0;
                state = DHCPState::Requesting;
                log_dhcp("state REQUESTING: received DHCPOFFER; sending DHCPREQUEST");
                transmit_message(true);
                attempts = 1;
                retry_at = ticks() + 100;
                return;
            }

            if (state != DHCPState::Requesting)
            {
                return;
            }
            if (options.has_server && !same_address(options.server, selected_server))
            {
                return;
            }
            if (options.message_type == 6)
            {
                log_dhcp("received DHCPNAK; restarting address discovery");
                begin_discovery();
                return;
            }
            if (options.message_type != 5)
            {
                return;
            }

            IPAddress acknowledged = {};
            for (uint8_t index = 0; index < 4; ++index)
            {
                acknowledged.bytes[index] = packet[16 + index];
            }
            if (!address_is_zero(acknowledged) && !same_address(acknowledged, offered_address))
            {
                return;
            }
            apply_configuration(packet, options);
        }
    }
}
