#include "driver.hpp"

#include "drivers/ethernet/driver.hpp"

#if defined(__x86_64__)
#include "drivers/memory/driver.hpp"
#include "drivers/pci/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "libs/asm/asm.hpp"
#endif

namespace VirtIONet
{
#if defined(__x86_64__)
    namespace
    {
        constexpr uint16_t QueueCapacity = 256;
        constexpr uint16_t MaximumFrameLength = 1514;
        constexpr uint16_t ReceiveBufferLength = 10 + MaximumFrameLength;
        constexpr uint32_t QueueMemoryLength = 16384;
        constexpr uint32_t VirtioNetMacFeature = 1u << 5;
        constexpr uint32_t VirtioNetStatusFeature = 1u << 16;
        constexpr uint8_t StatusAcknowledge = 1;
        constexpr uint8_t StatusDriver = 2;
        constexpr uint8_t StatusDriverOk = 4;
        constexpr uint16_t DescriptorWrite = 2;

        struct __attribute__((packed)) VirtqDescriptor
        {
            uint64_t address;
            uint32_t length;
            uint16_t flags;
            uint16_t next;
        };

        struct __attribute__((packed)) VirtqUsedElement
        {
            uint32_t id;
            uint32_t length;
        };

        struct Queue
        {
            uint8_t* memory;
            VirtqDescriptor* descriptors;
            volatile uint16_t* available_flags;
            volatile uint16_t* available_index;
            volatile uint16_t* available_ring;
            volatile uint16_t* used_flags;
            volatile uint16_t* used_index;
            VirtqUsedElement* used_ring;
            uint16_t size;
            uint16_t last_used;
        };

        alignas(4096) uint8_t receive_queue_memory[QueueMemoryLength];
        alignas(4096) uint8_t transmit_queue_memory[QueueMemoryLength];
        alignas(4096) uint8_t receive_buffers[QueueCapacity][ReceiveBufferLength];
        alignas(4096) uint8_t transmit_buffers[QueueCapacity][ReceiveBufferLength];

        Queue receive_queue = {};
        Queue transmit_queue = {};
        bool transmit_busy[QueueCapacity] = {};
        uint16_t transmit_cursor = 0;
        uint16_t io_base = 0;
        bool available = false;
        bool link_status_feature = false;
        bool last_link_up = false;
        uint32_t transmit_failures = 0;

        uint64_t physical_address(const void* address)
        {
            const uint64_t virtual_address = reinterpret_cast<uint64_t>(address);
            const uint64_t physical_page = Memory::vmm::translate(virtual_address);
            if (physical_page == 0)
            {
                return 0;
            }
            return physical_page | (virtual_address & (Memory::PAGE_SIZE - 1));
        }

        bool dma_range_is_contiguous(const void* address, uint32_t length)
        {
            const uint64_t virtual_address = reinterpret_cast<uint64_t>(address);
            const uint64_t first_page = virtual_address & ~(Memory::PAGE_SIZE - 1);
            const uint64_t first_physical = Memory::vmm::translate(virtual_address);
            if (first_physical == 0)
            {
                return false;
            }

            const uint64_t final_address = virtual_address + length - 1;
            const uint64_t final_page = final_address & ~(Memory::PAGE_SIZE - 1);
            for (uint64_t page = first_page + Memory::PAGE_SIZE; page <= final_page; page += Memory::PAGE_SIZE)
            {
                if (Memory::vmm::translate(page) != first_physical + (page - first_page))
                {
                    return false;
                }
            }
            return true;
        }

        bool valid_mac(const Ethernet::MACAddress& mac)
        {
            bool all_zero = true;
            bool all_ones = true;
            for (uint8_t byte : mac.bytes)
            {
                all_zero = all_zero && byte == 0;
                all_ones = all_ones && byte == 0xFF;
            }
            return !all_zero && !all_ones && (mac.bytes[0] & 1) == 0;
        }

        bool prepare_queue(Queue& queue, uint8_t* memory, uint16_t queue_index, bool receive)
        {
            outw(io_base + 0x0E, queue_index);
            const uint16_t device_size = inw(io_base + 0x0C);
            if (device_size < 8 || device_size > QueueCapacity || (device_size & (device_size - 1)) != 0)
            {
                Uart::puts("[VIRTIO-NET] Device reported an unusable queue size\n");
                return false;
            }

            queue.size = device_size;
            queue.memory = memory;
            queue.descriptors = reinterpret_cast<VirtqDescriptor*>(memory);
            const uint32_t available_offset = queue.size * sizeof(VirtqDescriptor);
            const uint32_t used_offset = (available_offset + 4 + queue.size * 2 + 2 + 4095) & ~4095u;
            queue.available_flags = reinterpret_cast<volatile uint16_t*>(memory + available_offset);
            queue.available_index = queue.available_flags + 1;
            queue.available_ring = queue.available_index + 1;
            queue.used_flags = reinterpret_cast<volatile uint16_t*>(memory + used_offset);
            queue.used_index = queue.used_flags + 1;
            queue.used_ring = reinterpret_cast<VirtqUsedElement*>(const_cast<uint16_t*>(queue.used_index + 1));
            queue.last_used = 0;

            if (used_offset + 4 + queue.size * sizeof(VirtqUsedElement) > QueueMemoryLength || !dma_range_is_contiguous(memory, used_offset + 4 + queue.size * sizeof(VirtqUsedElement)))
            {
                Uart::puts("[VIRTIO-NET] Queue memory is not DMA-contiguous\n");
                return false;
            }

            Memory::memset(memory, 0, QueueMemoryLength);
            if (receive)
            {
                for (uint16_t index = 0; index < queue.size; ++index)
                {
                    uint64_t buffer_address = physical_address(receive_buffers[index]);
                    if (!buffer_address || !dma_range_is_contiguous(receive_buffers[index], ReceiveBufferLength))
                    {
                        Uart::puts("[VIRTIO-NET] Receive buffer is not DMA-accessible\n");
                        return false;
                    }
                    queue.descriptors[index].address = buffer_address;
                    queue.descriptors[index].length = ReceiveBufferLength;
                    queue.descriptors[index].flags = DescriptorWrite;
                    queue.available_ring[index] = index;
                }
                *queue.available_index = queue.size;
            }
            else
            {
                for (uint16_t index = 0; index < queue.size; ++index)
                {
                    uint64_t buffer_address = physical_address(transmit_buffers[index]);
                    if (!buffer_address || !dma_range_is_contiguous(transmit_buffers[index], ReceiveBufferLength))
                    {
                        Uart::puts("[VIRTIO-NET] Transmit buffer is not DMA-accessible\n");
                        return false;
                    }
                    queue.descriptors[index].address = buffer_address;
                }
            }

            const uint64_t queue_address = physical_address(memory);
            if (!queue_address || (queue_address & 4095) != 0 || (queue_address >> 12) > 0xFFFFFFFFu)
            {
                Uart::puts("[VIRTIO-NET] Queue physical address is invalid for legacy VirtIO\n");
                return false;
            }
            outl(io_base + 0x08, static_cast<uint32_t>(queue_address >> 12));
            return true;
        }

        void reset_device()
        {
            if (io_base)
            {
                outb(io_base + 0x12, 0);
            }
            available = false;
            link_status_feature = false;
            last_link_up = false;
            io_base = 0;
        }

        bool initialize_legacy_device(uint8_t bus, uint8_t slot, uint8_t function,
                                      Ethernet::MACAddress& mac)
        {
            const uint32_t bar0 = Pci::config_read32(bus, slot, function, 0x10);
            if ((bar0 & 1) == 0)
            {
                return false;
            }
            io_base = static_cast<uint16_t>(bar0 & 0xFFFC);
            if (!io_base)
            {
                return false;
            }

            const uint16_t command = Pci::config_read16(bus, slot, function, 0x04);
            Pci::config_write16(bus, slot, function, 0x04, command | 0x0005);

            outb(io_base + 0x12, 0);
            if (inb(io_base + 0x12) != 0)
            {
                Uart::puts("[VIRTIO-NET] Device did not reset\n");
                reset_device();
                return false;
            }
            outb(io_base + 0x12, StatusAcknowledge | StatusDriver);

            const uint32_t device_features = inl(io_base);
            if ((device_features & VirtioNetMacFeature) == 0)
            {
                Uart::puts("[VIRTIO-NET] Device does not expose the MAC feature\n");
                reset_device();
                return false;
            }
            link_status_feature = (device_features & VirtioNetStatusFeature) != 0;
            const uint32_t negotiated_features = VirtioNetMacFeature | (link_status_feature ? VirtioNetStatusFeature : 0);
            outl(io_base + 0x04, negotiated_features);
            if (inb(io_base + 0x12) & 0x80)
            {
                Uart::puts("[VIRTIO-NET] Device rejected negotiated features\n");
                reset_device();
                return false;
            }

            for (uint8_t index = 0; index < 6; ++index)
            {
                mac.bytes[index] = inb(io_base + 0x14 + index);
            }
            if (!valid_mac(mac))
            {
                Uart::puts("[VIRTIO-NET] Device provided an invalid MAC address\n");
                reset_device();
                return false;
            }

            if (!prepare_queue(receive_queue, receive_queue_memory, 0, true) || !prepare_queue(transmit_queue, transmit_queue_memory, 1, false))
            {
                reset_device();
                return false;
            }

            Memory::memset(transmit_busy, 0, sizeof(transmit_busy));
            transmit_cursor = 0;
            outb(io_base + 0x12, StatusAcknowledge | StatusDriver | StatusDriverOk);
            if ((inb(io_base + 0x12) & StatusDriverOk) == 0 || (inb(io_base + 0x12) & 0x80) != 0)
            {
                Uart::puts("[VIRTIO-NET] Device failed to enter DRIVER_OK\n");
                reset_device();
                return false;
            }
            available = true;
            if (link_status_feature)
            {
                last_link_up = (inw(io_base + 0x1A) & 1) != 0;
                Uart::puts(last_link_up ? "[VIRTIO-NET] Link is up\n" : "[VIRTIO-NET] Link is down\n");
            }

            Uart::puts("[VIRTIO-NET] MAC ");
            constexpr char hex_digits[] = "0123456789ABCDEF";
            for (uint8_t index = 0; index < 6; ++index)
            {
                Uart::putc(hex_digits[mac.bytes[index] >> 4]);
                Uart::putc(hex_digits[mac.bytes[index] & 0x0F]);
                if (index != 5)
                {
                    Uart::putc(':');
                }
            }
            Uart::puts("; RX/TX queues ready\n");
            return true;
        }

        void reclaim_transmit_descriptors()
        {
            asm volatile("" ::: "memory");
            while (transmit_queue.last_used != *transmit_queue.used_index)
            {
                const uint16_t slot = transmit_queue.last_used % transmit_queue.size;
                const uint32_t descriptor = transmit_queue.used_ring[slot].id;
                if (descriptor < transmit_queue.size)
                {
                    transmit_busy[descriptor] = false;
                }
                transmit_queue.last_used++;
            }
        }
    }

    bool init(Ethernet::MACAddress& mac)
    {
        reset_device();
        Uart::puts("[VIRTIO-NET] Scanning PCI for a legacy VirtIO network device\n");

        for (uint16_t bus = 0; bus < 256; ++bus)
        {
            for (uint8_t slot = 0; slot < 32; ++slot)
            {
                const uint16_t vendor = Pci::config_read16(static_cast<uint8_t>(bus), slot, 0, 0);
                if (vendor == 0xFFFF)
                {
                    continue;
                }
                const uint8_t function_count = (Pci::config_read8(static_cast<uint8_t>(bus), slot, 0, 0x0E) & 0x80) ? 8 : 1;
                for (uint8_t function = 0; function < function_count; ++function)
                {
                    const uint16_t function_vendor = Pci::config_read16(static_cast<uint8_t>(bus), slot, function, 0);
                    if (function_vendor == 0xFFFF)
                    {
                        continue;
                    }
                    const uint16_t device = Pci::config_read16(static_cast<uint8_t>(bus), slot, function, 2);
                    if (function_vendor == 0x1AF4 && device == 0x1000)
                    {
                        Uart::puts("[VIRTIO-NET] Found transitional VirtIO network PCI function\n");
                        if (initialize_legacy_device(static_cast<uint8_t>(bus), slot, function, mac))
                        {
                            return true;
                        }
                        Uart::puts("[VIRTIO-NET] Device initialization failed\n");
                    }
                    else if (function_vendor == 0x1AF4 && device == 0x1041)
                    {
                        Uart::puts("[VIRTIO-NET] Modern-only PCI transport is not supported by this legacy driver\n");
                    }
                }
            }
        }
        reset_device();
        Uart::puts("[VIRTIO-NET] No usable network device\n");
        return false;
    }

    bool is_available()
    {
        return available;
    }

    bool send(const uint8_t* frame, uint16_t length)
    {
        if (!available || !frame || length == 0 || length > MaximumFrameLength)
        {
            return false;
        }
        reclaim_transmit_descriptors();

        uint16_t descriptor = transmit_cursor;
        bool found = false;
        for (uint16_t attempt = 0; attempt < transmit_queue.size; ++attempt)
        {
            descriptor = static_cast<uint16_t>((transmit_cursor + attempt) % transmit_queue.size);
            if (!transmit_busy[descriptor])
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            ++transmit_failures;
            if ((transmit_failures & 0x3F) == 1)
            {
                Uart::puts("[VIRTIO-NET] Transmit queue has no available descriptor\n");
            }
            return false;
        }

        uint8_t* buffer = transmit_buffers[descriptor];
        Memory::memset(buffer, 0, 10);
        Memory::memcpy(buffer + 10, frame, length);
        transmit_queue.descriptors[descriptor].length = length + 10;
        transmit_busy[descriptor] = true;
        transmit_cursor = static_cast<uint16_t>((descriptor + 1) % transmit_queue.size);

        const uint16_t available_index = *transmit_queue.available_index;
        transmit_queue.available_ring[available_index % transmit_queue.size] = descriptor;
        asm volatile("" ::: "memory");
        *transmit_queue.available_index = available_index + 1;
        outw(io_base + 0x10, 1);
        return true;
    }

    void poll(Ethernet::ReceiveHandler receive)
    {
        if (!available || !receive)
        {
            return;
        }
        reclaim_transmit_descriptors();

        asm volatile("" ::: "memory");
        while (receive_queue.last_used != *receive_queue.used_index)
        {
            const uint16_t slot = receive_queue.last_used % receive_queue.size;
            const VirtqUsedElement element = receive_queue.used_ring[slot];
            if (element.id < receive_queue.size && element.length >= 10 && element.length <= ReceiveBufferLength)
            {
                const uint16_t frame_length = static_cast<uint16_t>(element.length - 10);
                receive(receive_buffers[element.id] + 10, frame_length);
                receive_queue.available_ring[*receive_queue.available_index % receive_queue.size] = static_cast<uint16_t>(element.id);
                asm volatile("" ::: "memory");
                *receive_queue.available_index = *receive_queue.available_index + 1;
                outw(io_base + 0x10, 0);
            }
            else
            {
                Uart::puts("[VIRTIO-NET] Dropped malformed receive descriptor\n");
                if (element.id < receive_queue.size)
                {
                    receive_queue.available_ring[*receive_queue.available_index % receive_queue.size] = static_cast<uint16_t>(element.id);
                    *receive_queue.available_index = *receive_queue.available_index + 1;
                    outw(io_base + 0x10, 0);
                }
            }
            receive_queue.last_used++;
        }
        if (link_status_feature)
        {
            const bool link_up = (inw(io_base + 0x1A) & 1) != 0;
            if (link_up != last_link_up)
            {
                last_link_up = link_up;
                Uart::puts(link_up ? "[VIRTIO-NET] Link is up\n" : "[VIRTIO-NET] Link is down\n");
            }
        }
    }
#else
    bool init(Ethernet::MACAddress&)
    {
        return false;
    }

    bool is_available()
    {
        return false;
    }

    bool send(const uint8_t*, uint16_t)
    {
        return false;
    }

    void poll(Ethernet::ReceiveHandler)
    {
    }
#endif
}
