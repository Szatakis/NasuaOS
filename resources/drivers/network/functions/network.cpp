#include "../driver.hpp"

#include "drivers/ethernet/driver.hpp"
#include "drivers/gpu/driver.hpp"
#include "drivers/timer/driver.hpp"
#include "libs/libc/libc.hpp"
#include "network_internal.hpp"

namespace Network
{
    bool available = false;
    MACAddress mac_addr = {};
    IPAddress ip_addr = {};
    IPAddress netmask = {};
    IPAddress gateway = {};
    IPAddress dns_server = {};
    uint64_t rx_packets = 0;
    uint64_t tx_packets = 0;

    namespace Internal
    {
        uint64_t ticks()
        {
#if defined(__x86_64__)
            return Timer::pit_get_ticks();
#else
            return 0;
#endif
        }

        bool send_frame(const uint8_t* frame, uint16_t length)
        {
            if (!available || !Ethernet::send_frame(frame, length))
            {
                return false;
            }
            ++tx_packets;
            return true;
        }

        void handle_frame(const uint8_t* frame, uint16_t length)
        {
            if (!frame || length < 14)
            {
                return;
            }
            ++rx_packets;
            const uint16_t ether_type = read_be16(frame + 12);
            if (ether_type == 0x0806)
            {
                handle_arp(frame + 14, static_cast<uint16_t>(length - 14));
            }
            else if (ether_type == 0x0800)
            {
                handle_ipv4(frame + 14, static_cast<uint16_t>(length - 14));
            }
        }
    }

    void init()
    {
        available = false;
        mac_addr = {};
        ip_addr = {};
        netmask = {};
        gateway = {};
        dns_server = {};
        rx_packets = 0;
        tx_packets = 0;

        if (!Ethernet::init())
        {
            return;
        }
        const Ethernet::MACAddress mac = Ethernet::get_mac();
        for (uint8_t index = 0; index < 6; ++index)
        {
            mac_addr.bytes[index] = mac.bytes[index];
        }
        available = true;
        Internal::dhcp_start();
    }

    bool is_available()
    {
        return available;
    }

    IPAddress get_ip()
    {
        return ip_addr;
    }

    void set_ip(IPAddress ip)
    {
        ip_addr = ip;
    }

    IPAddress get_netmask()
    {
        return netmask;
    }

    void set_netmask(IPAddress mask)
    {
        netmask = mask;
    }

    IPAddress get_gateway()
    {
        return gateway;
    }

    void set_gateway(IPAddress gw)
    {
        gateway = gw;
    }

    IPAddress get_dns_server()
    {
        return dns_server;
    }

    void set_dns_server(IPAddress dns)
    {
        dns_server = dns;
    }

    MACAddress get_mac()
    {
        return mac_addr;
    }

    void poll()
    {
        if (!available)
        {
            return;
        }
        Ethernet::poll(Internal::handle_frame);
        Internal::dhcp_poll();
    }

    bool send_packet(const uint8_t* payload, uint16_t length)
    {
        return Internal::send_frame(payload, length);
    }

    void print_info()
    {
#if defined(__x86_64__)
        char buffer[64];
        Gpu::print_info("Network Interface Info\n");
#else
        char buffer[64];
#endif
        Gpu::print("Status: ");
        Gpu::print(available ? "UP\n" : "DOWN (No Device)\n");

        Gpu::print("MAC Address: ");
        for (uint8_t index = 0; index < 6; ++index)
        {
            itoa(mac_addr.bytes[index], buffer);
            Gpu::print(buffer);
            Gpu::print(index == 5 ? "\n" : ":");
        }

        const char* labels[] = {"IP Address: ", "Subnet Mask: ", "Gateway: ", "DNS Server: "};
        const IPAddress* addresses[] = {&ip_addr, &netmask, &gateway, &dns_server};
        for (uint8_t field = 0; field < 4; ++field)
        {
            Gpu::print(labels[field]);
            for (uint8_t index = 0; index < 4; ++index)
            {
                itoa(addresses[field]->bytes[index], buffer);
                Gpu::print(buffer);
                Gpu::print(index == 3 ? "\n" : ".");
            }
        }

        Gpu::print("Packets Sent: ");
        itoa(tx_packets, buffer);
        Gpu::print(buffer);
        Gpu::print(" | Packets Received: ");
        itoa(rx_packets, buffer);
        Gpu::print(buffer);
        Gpu::print("\n");
    }
}
