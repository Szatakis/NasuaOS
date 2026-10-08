#include "../driver.hpp"

#include <stdint.h>
#include <stddef.h>

#include "drivers/pci/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "drivers/gpu/driver.hpp"
#include "libs/asm/asm.hpp"
#include "libs/libc/libc.hpp"
#include "system/sysfunc/logger/logger.hpp"

namespace Network
{
    bool available = false;
    MACAddress mac_addr = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    IPAddress ip_addr = {{192, 168, 100, 2}};
    IPAddress netmask = {{255, 255, 255, 0}};
    IPAddress gateway = {{192, 168, 100, 1}};
    uint64_t rx_packets = 0;
    uint64_t tx_packets = 0;

    static uint16_t io_base = 0;
    static uint8_t pci_bus = 0;
    static uint8_t pci_slot = 0;
    static uint8_t pci_fn = 0;

    // Ethernet Header
    struct __attribute__((packed)) EthernetHeader
    {
        uint8_t dst_mac[6];
        uint8_t src_mac[6];
        uint16_t ethertype; // 0x0800 IPv4, 0x0806 ARP
    };

    // ARP Header
    struct __attribute__((packed)) ArpHeader
    {
        uint16_t hw_type;    // 1 = Ethernet
        uint16_t proto_type; // 0x0800 = IPv4
        uint8_t  hw_len;     // 6
        uint8_t  proto_len;  // 4
        uint16_t opcode;     // 1 = Request, 2 = Reply
        uint8_t  src_mac[6];
        uint8_t  src_ip[4];
        uint8_t  dst_mac[6];
        uint8_t  dst_ip[4];
    };

    // IPv4 Header
    struct __attribute__((packed)) Ipv4Header
    {
        uint8_t  ver_ihl;     // 0x45
        uint8_t  tos;
        uint16_t total_len;
        uint16_t id;
        uint16_t fragment;
        uint8_t  ttl;         // 64
        uint8_t  protocol;    // 1 = ICMP
        uint16_t checksum;
        uint8_t  src_ip[4];
        uint8_t  dst_ip[4];
    };

    // ICMP Header
    struct __attribute__((packed)) IcmpHeader
    {
        uint8_t  type; // 8 = Echo Request, 0 = Echo Reply
        uint8_t  code; // 0
        uint16_t checksum;
        uint16_t id;
        uint16_t sequence;
    };

    // VirtIO Net Header
    struct __attribute__((packed)) VirtioNetHeader
    {
        uint8_t flags;
        uint8_t gso_type;
        uint16_t hdr_len;
        uint16_t gso_size;
        uint16_t csum_start;
        uint16_t csum_offset;
    };

    // Simple Virtqueue Legacy structure for buffer submission
    struct __attribute__((packed)) VirtqDesc
    {
        uint64_t addr;
        uint32_t len;
        uint16_t flags; // 1=NEXT, 2=WRITE
        uint16_t next;
    };

    struct __attribute__((packed)) VirtqAvail
    {
        uint16_t flags;
        uint16_t idx;
        uint16_t ring[16];
    };

    struct __attribute__((packed)) VirtqUsedItem
    {
        uint32_t id;
        uint32_t len;
    };

    struct __attribute__((packed)) VirtqUsed
    {
        uint16_t flags;
        uint16_t idx;
        VirtqUsedItem ring[16];
    };

    // Ring buffers (aligned to page boundaries for VirtIO)
    alignas(4096) static VirtqDesc rx_desc[16];
    alignas(4096) static VirtqAvail rx_avail;
    alignas(4096) static VirtqUsed rx_used;

    alignas(4096) static VirtqDesc tx_desc[16];
    alignas(4096) static VirtqAvail tx_avail;
    alignas(4096) static VirtqUsed tx_used;

    alignas(4096) static uint8_t rx_buffer_pool[16][2048];
    alignas(4096) static uint8_t tx_buffer_pool[16][2048];

    static uint16_t rx_last_used_idx = 0;
    static bool ping_reply_received = false;
    static uint16_t ping_reply_seq = 0;

    static uint16_t htons(uint16_t val)
    {
        return (uint16_t)((val << 8) | (val >> 8));
    }

    static uint16_t ntohs(uint16_t val)
    {
        return htons(val);
    }

    static uint16_t calculate_checksum(const void* data, size_t length)
    {
        uint32_t sum = 0;
        const uint16_t* ptr = (const uint16_t*)data;

        while (length > 1)
        {
            sum += *ptr++;
            length -= 2;
        }

        if (length == 1)
        {
            sum += *(const uint8_t*)ptr;
        }

        while (sum >> 16)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        return (uint16_t)(~sum);
    }

    static void setup_virtqueue(uint16_t queue_index)
    {
        outw(io_base + 0x0E, queue_index); // Queue Select
        uint16_t queue_size = inw(io_base + 0x0C);
        if (queue_size == 0) 
        {
            return;
        }

        if (queue_index == 0) // RX Queue
        {
            Memory::memset(rx_desc, 0, sizeof(rx_desc));
            Memory::memset(&rx_avail, 0, sizeof(rx_avail));
            Memory::memset(&rx_used, 0, sizeof(rx_used));

            for (int i = 0; i < 16; i++)
            {
                rx_desc[i].addr = (uint64_t)&rx_buffer_pool[i][0];
                rx_desc[i].len = 2048;
                rx_desc[i].flags = 2; // VIRTQ_DESC_F_WRITE (Device writes into buffer)
                rx_desc[i].next = 0;
                rx_avail.ring[i] = i;
            }
            rx_avail.idx = 16;

            uint64_t pfn = (uint64_t)&rx_desc[0] / 4096;
            outl(io_base + 0x08, (uint32_t)pfn);
        }
        else if (queue_index == 1) // TX Queue
        {
            Memory::memset(tx_desc, 0, sizeof(tx_desc));
            Memory::memset(&tx_avail, 0, sizeof(tx_avail));
            Memory::memset(&tx_used, 0, sizeof(tx_used));

            for (int i = 0; i < 16; i++)
            {
                tx_desc[i].addr = (uint64_t)&tx_buffer_pool[i][0];
                tx_desc[i].len = 2048;
                tx_desc[i].flags = 0;
                tx_desc[i].next = 0;
            }

            uint64_t pfn = (uint64_t)&tx_desc[0] / 4096;
            outl(io_base + 0x08, (uint32_t)pfn);
        }
    }

    void init()
    {
        Uart::puts("[NET] Initializing VirtIO Network Driver...\n");
        available = false;

        // Scan PCI for VirtIO Net (Vendor 0x1AF4, Device 0x1000 or Class 0x02 Network)
        for (uint16_t bus = 0; bus < 256; bus++)
        {
            for (uint8_t slot = 0; slot < 32; slot++)
            {
                for (uint8_t fn = 0; fn < 8; fn++)
                {
                    uint16_t vendor = Pci::config_read16((uint8_t)bus, slot, fn, 0x00);
                    if (vendor == 0xFFFF) 
                    {
                        continue;
                    }

                    uint16_t device = Pci::config_read16((uint8_t)bus, slot, fn, 0x02);
                    uint8_t class_code = Pci::config_read8((uint8_t)bus, slot, fn, 0x0B);

                    if (vendor == 0x1AF4 && (device == 0x1000 || device == 0x1041 || class_code == 0x02))
                    {
                        pci_bus = (uint8_t)bus;
                        pci_slot = slot;
                        pci_fn = fn;

                        uint32_t bar0 = Pci::config_read32(pci_bus, pci_slot, pci_fn, 0x10);
                        if (bar0 & 1)
                        {
                            io_base = (uint16_t)(bar0 & ~3);
                        }
                        else
                        {
                            io_base = 0xC000; // Default fallback QEMU VirtIO I/O port
                        }

                        // Enable Bus Master (bit 2) and I/O Space (bit 0)
                        uint16_t cmd = Pci::config_read16(pci_bus, pci_slot, pci_fn, 0x04);
                        Pci::config_write16(pci_bus, pci_slot, pci_fn, 0x04, cmd | 0x05);

                        Uart::puts("[NET] VirtIO Net PCI device found at IO Base: 0x");
                        char hex_buf[16];
                        itoa(io_base, hex_buf);
                        Uart::puts(hex_buf);
                        Uart::puts("\n");

                        // Reset VirtIO device
                        outb(io_base + 0x12, 0x00);
                        // Acknowledge device
                        outb(io_base + 0x12, 0x01 | 0x02); // ACK (1) | DRIVER (2)

                        // Read MAC address from VirtIO config space (offset 0x14)
                        for (int i = 0; i < 6; i++)
                        {
                            mac_addr.bytes[i] = inb(io_base + 0x14 + i);
                        }

                        // Setup Virtqueues (0 = RX, 1 = TX)
                        setup_virtqueue(0);
                        setup_virtqueue(1);

                        // Set DRIVER_OK (4) status
                        outb(io_base + 0x12, 0x01 | 0x02 | 0x04);

                        available = true;
                        log(INFO, "NET", "VirtIO Network Driver initialized successfully");
                        break;
                    }
                }
                if (available) 
                {
                    break;
                }
            }
            if (available) 
            {
                break;
            }
        }

        if (!available)
        {
            Uart::puts("[NET] VirtIO Network device not detected on PCI bus.\n");
            log(WARN, "NET", "VirtIO Network device not detected");
        }
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
    MACAddress get_mac() 
    { 
        return mac_addr; 
    }

    bool send_packet(const uint8_t* payload, uint16_t length)
    {
        if (!available || io_base == 0 || length > 1514) 
        {
            return false;
        }

        static uint16_t tx_idx = 0;
        uint16_t desc_idx = tx_idx % 16;

        VirtioNetHeader virtio_hdr;
        Memory::memset(&virtio_hdr, 0, sizeof(virtio_hdr));

        uint8_t* buf = &tx_buffer_pool[desc_idx][0];
        Memory::memcpy(buf, &virtio_hdr, sizeof(VirtioNetHeader));
        Memory::memcpy(buf + sizeof(VirtioNetHeader), payload, length);

        tx_desc[desc_idx].addr = (uint64_t)buf;
        tx_desc[desc_idx].len = sizeof(VirtioNetHeader) + length;
        tx_desc[desc_idx].flags = 0;

        tx_avail.ring[tx_avail.idx % 16] = desc_idx;
        tx_avail.idx++;
        tx_idx++;
        tx_packets++;

        outw(io_base + 0x10, 1); // Notify TX queue (queue 1)
        return true;
    }

    static void handle_packet(const uint8_t* packet, uint16_t length)
    {
        if (length < sizeof(EthernetHeader)) 
        {
            return;
        }
        rx_packets++;

        const EthernetHeader* eth = (const EthernetHeader*)packet;
        uint16_t ethertype = ntohs(eth->ethertype);

        // Process ARP Request (0x0806)
        if (ethertype == 0x0806 && length >= sizeof(EthernetHeader) + sizeof(ArpHeader))
        {
            const ArpHeader* arp = (const ArpHeader*)(packet + sizeof(EthernetHeader));
            if (ntohs(arp->opcode) == 1 && Memory::memcmp(arp->dst_ip, ip_addr.bytes, 4) == 0)
            {
                // Form ARP Reply
                uint8_t reply_buf[sizeof(EthernetHeader) + sizeof(ArpHeader)];
                EthernetHeader* reply_eth = (EthernetHeader*)reply_buf;
                ArpHeader* reply_arp = (ArpHeader*)(reply_buf + sizeof(EthernetHeader));

                Memory::memcpy(reply_eth->dst_mac, eth->src_mac, 6);
                Memory::memcpy(reply_eth->src_mac, mac_addr.bytes, 6);
                reply_eth->ethertype = htons(0x0806);

                reply_arp->hw_type = htons(1);
                reply_arp->proto_type = htons(0x0800);
                reply_arp->hw_len = 6;
                reply_arp->proto_len = 4;
                reply_arp->opcode = htons(2); // Reply
                Memory::memcpy(reply_arp->src_mac, mac_addr.bytes, 6);
                Memory::memcpy(reply_arp->src_ip, ip_addr.bytes, 4);
                Memory::memcpy(reply_arp->dst_mac, arp->src_mac, 6);
                Memory::memcpy(reply_arp->dst_ip, arp->src_ip, 4);

                send_packet(reply_buf, sizeof(reply_buf));
            }
        }
        // Process IPv4 (0x0800) -> ICMP Ping Echo Request / Reply
        else if (ethertype == 0x0800 && length >= sizeof(EthernetHeader) + sizeof(Ipv4Header))
        {
            const Ipv4Header* ip = (const Ipv4Header*)(packet + sizeof(EthernetHeader));
            if (ip->protocol == 1 && Memory::memcmp(ip->dst_ip, ip_addr.bytes, 4) == 0) // ICMP
            {
                uint16_t ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
                const IcmpHeader* icmp = (const IcmpHeader*)(packet + sizeof(EthernetHeader) + ip_hdr_len);
                uint16_t icmp_len = ntohs(ip->total_len) - ip_hdr_len;

                // Handle ICMP Echo Request (Type 8 -> Reply Type 0)
                if (icmp->type == 8 && icmp_len >= sizeof(IcmpHeader))
                {
                    uint8_t reply_buf[1514];
                    uint16_t total_reply_len = sizeof(EthernetHeader) + ntohs(ip->total_len);
                    if (total_reply_len <= sizeof(reply_buf))
                    {
                        Memory::memcpy(reply_buf, packet, total_reply_len);

                        EthernetHeader* reply_eth = (EthernetHeader*)reply_buf;
                        Ipv4Header* reply_ip = (Ipv4Header*)(reply_buf + sizeof(EthernetHeader));
                        IcmpHeader* reply_icmp = (IcmpHeader*)(reply_buf + sizeof(EthernetHeader) + ip_hdr_len);

                        Memory::memcpy(reply_eth->dst_mac, eth->src_mac, 6);
                        Memory::memcpy(reply_eth->src_mac, mac_addr.bytes, 6);

                        Memory::memcpy(reply_ip->dst_ip, ip->src_ip, 4);
                        Memory::memcpy(reply_ip->src_ip, ip_addr.bytes, 4);
                        reply_ip->checksum = 0;
                        reply_ip->checksum = calculate_checksum(reply_ip, ip_hdr_len);

                        reply_icmp->type = 0; // Echo Reply
                        reply_icmp->checksum = 0;
                        reply_icmp->checksum = calculate_checksum(reply_icmp, icmp_len);

                        send_packet(reply_buf, total_reply_len);
                    }
                }
                // Handle ICMP Echo Reply (Type 0 for ping command)
                else if (icmp->type == 0)
                {
                    ping_reply_received = true;
                    ping_reply_seq = ntohs(icmp->sequence);
                }
            }
        }
    }

    void poll()
    {
        if (!available) 
        {
            return;
        }

        // Check Virtqueue RX used ring
        while (rx_last_used_idx != rx_used.idx)
        {
            uint16_t used_slot = rx_last_used_idx % 16;
            VirtqUsedItem item = rx_used.ring[used_slot];

            if (item.len > sizeof(VirtioNetHeader))
            {
                uint8_t* pkt = &rx_buffer_pool[item.id][sizeof(VirtioNetHeader)];
                uint16_t pkt_len = (uint16_t)(item.len - sizeof(VirtioNetHeader));
                handle_packet(pkt, pkt_len);
            }

            // Re-queue RX descriptor
            rx_avail.ring[rx_avail.idx % 16] = item.id;
            rx_avail.idx++;
            rx_last_used_idx++;

            outw(io_base + 0x10, 0); // Notify RX queue
        }
    }

    bool ping(IPAddress target_ip, uint32_t timeout_ms)
    {
        if (!available) 
        {
            return false;
        }

        ping_reply_received = false;

        // Construct broadcast Ethernet + IPv4 + ICMP Echo Request frame
        uint8_t pkt[sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(IcmpHeader) + 32];
        Memory::memset(pkt, 0, sizeof(pkt));

        EthernetHeader* eth = (EthernetHeader*)pkt;
        Memory::memset(eth->dst_mac, 0xFF, 6); // Broadcast MAC
        Memory::memcpy(eth->src_mac, mac_addr.bytes, 6);
        eth->ethertype = htons(0x0800);

        Ipv4Header* ip = (Ipv4Header*)(pkt + sizeof(EthernetHeader));
        ip->ver_ihl = 0x45;
        ip->total_len = htons(sizeof(Ipv4Header) + sizeof(IcmpHeader) + 32);
        ip->ttl = 64;
        ip->protocol = 1; // ICMP
        Memory::memcpy(ip->src_ip, ip_addr.bytes, 4);
        Memory::memcpy(ip->dst_ip, target_ip.bytes, 4);
        ip->checksum = calculate_checksum(ip, sizeof(Ipv4Header));

        IcmpHeader* icmp = (IcmpHeader*)(pkt + sizeof(EthernetHeader) + sizeof(Ipv4Header));
        icmp->type = 8; // Echo Request
        icmp->code = 0;
        icmp->id = htons(0x1234);
        icmp->sequence = htons(1);

        uint8_t* data = pkt + sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(IcmpHeader);
        for (int i = 0; i < 32; i++) 
        {
            data[i] = 'A' + (i % 26);
        }

        icmp->checksum = calculate_checksum(icmp, sizeof(IcmpHeader) + 32);

        send_packet(pkt, sizeof(pkt));

        // Poll for reply
        for (uint32_t i = 0; i < timeout_ms * 1000; i++)
        {
            poll();
            if (ping_reply_received)
            {
                return true;
            }
            io_wait();
        }

        return false;
    }

    void print_info()
    {
        char buf[64];
        Gpu::print_info("--- Network Interface Info ---\n");

        Gpu::print_info("Status: ");
        Gpu::print(available ? "UP (VirtIO Net)\n" : "DOWN (No Device)\n");

        Gpu::print_info("MAC Address: ");
        for (int i = 0; i < 6; i++)
        {
            itoa(mac_addr.bytes[i], buf);
            Gpu::print(buf);
            if (i < 5) 
            {
                Gpu::print(":");
            }
        }
        Gpu::print("\n");

        Gpu::print_info("IP Address: ");
        for (int i = 0; i < 4; i++)
        {
            itoa(ip_addr.bytes[i], buf);
            Gpu::print(buf);
            if (i < 3) 
            {
                Gpu::print(".");
            }
        }
        Gpu::print("\n");

        Gpu::print_info("Subnet Mask: ");
        for (int i = 0; i < 4; i++)
        {
            itoa(netmask.bytes[i], buf);
            Gpu::print(buf);
            if (i < 3) 
            {
                Gpu::print(".");
            }
        }
        Gpu::print("\n");

        Gpu::print_info("Gateway: ");
        for (int i = 0; i < 4; i++)
        {
            itoa(gateway.bytes[i], buf);
            Gpu::print(buf);
            if (i < 3) 
            {
                Gpu::print(".");
            }
        }
        Gpu::print("\n");

        Gpu::print_info("Packets Sent: ");
        itoa(tx_packets, buf);
        Gpu::print(buf);
        Gpu::print(" | Packets Received: ");
        itoa(rx_packets, buf);
        Gpu::print(buf);
        Gpu::print("\n");
    }
}