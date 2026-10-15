#include "network_internal.hpp"

#include "drivers/ethernet/driver.hpp"
#include "libs/asm/asm.hpp"
#if defined(__x86_64__)
#include "drivers/rtc/driver.hpp"
#include <bearssl.h>
extern "C" {
extern const br_x509_trust_anchor TAs[121];
extern const unsigned int nasua_mozilla_trust_anchor_count;
}
#endif

namespace Network
{
    namespace
    {
        constexpr uint16_t tcp_header_size = 20;
        constexpr uint16_t tcp_max_payload = 1460;
        constexpr uint16_t tcp_buffered_payload = 1480;
        constexpr uint16_t tcp_local_port_start = 49152;
        constexpr uint32_t tcp_receive_queue_size = 16384;

        struct TCPClient
        {
            bool active;
            bool syn_acknowledged;
            bool reset;
            bool request_acknowledged;
            bool peer_closed;
            bool local_fin_sent;
            bool overflow;
            bool request_sent;
            bool stream_mode;
            IPAddress remote_address;
            uint16_t local_port;
            uint16_t remote_port;
            uint16_t peer_mss;
            uint32_t initial_sequence;
            uint32_t next_sequence;
            uint32_t next_remote_sequence;
            uint32_t request_sequence;
            uint8_t* response;
            uint32_t response_capacity;
            uint32_t response_length;
            uint32_t transmit_acknowledged;
            uint32_t receive_head;
            uint32_t receive_length;
            uint8_t receive_queue[tcp_receive_queue_size];
        };

        TCPClient client = {};
        uint16_t next_local_port = tcp_local_port_start;
#if defined(__x86_64__)
        br_ssl_client_context tls_client;
        br_x509_minimal_context tls_x509;
        br_sslio_context tls_io;
        uint8_t tls_buffer[32768];
        uint8_t tls_read_buffer[512];
        uint64_t tls_deadline_tick;

#endif

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

        uint16_t tcp_checksum(const IPAddress& source, const IPAddress& destination, const uint8_t* segment, uint16_t length)
        {
            uint8_t pseudo_packet[12 + tcp_buffered_payload] = {};
            for (uint8_t index = 0; index < 4; ++index)
            {
                pseudo_packet[index] = source.bytes[index];
                pseudo_packet[4 + index] = destination.bytes[index];
            }
            pseudo_packet[9] = 6;
            Internal::write_be16(pseudo_packet + 10, length);
            for (uint16_t index = 0; index < length; ++index)
            {
                pseudo_packet[12 + index] = segment[index];
            }
            return Internal::checksum(pseudo_packet, 12 + length);
        }

        bool send_segment(uint8_t flags, uint32_t sequence, const uint8_t* payload = nullptr, uint16_t payload_length = 0)
        {
            if ((!payload && payload_length != 0) || payload_length > tcp_max_payload)
            {
                return false;
            }

            uint8_t segment[tcp_header_size + tcp_max_payload] = {};
            Internal::write_be16(segment, client.local_port);
            Internal::write_be16(segment + 2, client.remote_port);
            Internal::write_be32(segment + 4, sequence);
            Internal::write_be32(segment + 8, client.next_remote_sequence);
            segment[12] = 5 << 4;
            segment[13] = flags;
            Internal::write_be16(segment + 14, 32768);
            for (uint16_t index = 0; index < payload_length; ++index)
            {
                segment[tcp_header_size + index] = payload[index];
            }
            Internal::write_be16(segment + 16, tcp_checksum(ip_addr, client.remote_address, segment, static_cast<uint16_t>(tcp_header_size + payload_length)));
            return Internal::send_ipv4(ip_addr, client.remote_address, 6, segment, static_cast<uint16_t>(tcp_header_size + payload_length));
        }

        uint16_t peer_mss_from_options(const uint8_t* options, uint16_t length)
        {
            uint16_t mss = tcp_max_payload;
            for (uint16_t index = 0; index < length;)
            {
                const uint8_t kind = options[index];
                if (kind == 0)
                {
                    break;
                }
                if (kind == 1)
                {
                    ++index;
                    continue;
                }
                if (index + 1 >= length || options[index + 1] < 2 || options[index + 1] > length - index)
                {
                    break;
                }
                const uint8_t option_length = options[index + 1];
                if (kind == 2 && option_length == 4)
                {
                    const uint16_t advertised = Internal::read_be16(options + index + 2);
                    if (advertised >= 536 && advertised < mss)
                    {
                        mss = advertised;
                    }
                }
                index = static_cast<uint16_t>(index + option_length);
            }
            return mss;
        }

        bool parse_ipv4_address(const char* text, IPAddress& address)
        {
            uint8_t part = 0;
            const char* cursor = text;
            while (part < 4)
            {
                if (*cursor < '0' || *cursor > '9')
                {
                    return false;
                }
                uint16_t value = 0;
                uint8_t digits = 0;
                while (*cursor >= '0' && *cursor <= '9')
                {
                    value = static_cast<uint16_t>(value * 10 + (*cursor - '0'));
                    if (value > 255 || ++digits > 3)
                    {
                        return false;
                    }
                    ++cursor;
                }
                address.bytes[part++] = static_cast<uint8_t>(value);
                if (part < 4)
                {
                    if (*cursor != '.')
                    {
                        return false;
                    }
                    ++cursor;
                }
            }
            return *cursor == '\0';
        }

        bool valid_http_text(const char* text, bool require_path)
        {
            if (!text || (require_path && text[0] != '/'))
            {
                return false;
            }
            for (uint32_t index = 0; text[index] != '\0'; ++index)
            {
                const uint8_t character = static_cast<uint8_t>(text[index]);
                if (character < 32 || character == 127 || character == ' ')
                {
                    return false;
                }
            }
            return true;
        }

        bool append_request(char* output, uint32_t capacity, uint32_t& length, const char* text)
        {
            while (*text != '\0')
            {
                if (length + 1 >= capacity)
                {
                    return false;
                }
                output[length++] = *text++;
            }
            output[length] = '\0';
            return true;
        }

        void poll_network()
        {
            Ethernet::poll(Internal::handle_frame);
            Internal::dhcp_poll();
        }

#if defined(__x86_64__)
        bool secure_random_bytes(uint8_t* output, uint32_t length)
        {
            uint32_t eax = 0;
            uint32_t ebx = 0;
            uint32_t ecx = 0;
            uint32_t edx = 0;
            asm volatile("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(1), "c"(0));
            if ((ecx & (1u << 30)) == 0)
            {
                return false;
            }

            uint32_t offset = 0;
            while (offset < length)
            {
                uint32_t value = 0;
                unsigned char success = 0;
                bool generated = false;
                for (uint32_t attempt = 0; attempt < 10 && !generated; ++attempt)
                {
                    asm volatile("rdrand %0; setc %1"
                                 : "=r"(value), "=qm"(success)
                                 :
                                 : "cc");
                    generated = success != 0;
                }
                if (!generated)
                {
                    return false;
                }
                for (uint32_t byte = 0; byte < sizeof(value) && offset < length; ++byte)
                {
                    output[offset++] = static_cast<uint8_t>(value >> (byte * 8));
                }
            }
            return true;
        }

        static bool is_leap_year(uint32_t year)
        {
            return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        }

        static bool get_certificate_time(uint32_t& days, uint32_t& seconds)
        {
            const Rtc::RtcTime time = Rtc::get_time();
            if (time.year < 2020 || time.year > 2099 || time.month < 1 || time.month > 12 ||
                time.hour > 23 || time.minute > 59 || time.second > 59)
            {
                return false;
            }

            static const uint8_t month_lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            uint32_t month_length = month_lengths[time.month - 1];
            if (time.month == 2 && is_leap_year(time.year))
            {
                ++month_length;
            }
            if (time.day == 0 || time.day > month_length)
            {
                return false;
            }

            days = 0;
            for (uint32_t year = 1970; year < time.year; ++year)
            {
                days += is_leap_year(year) ? 366 : 365;
            }
            for (uint32_t month = 1; month < time.month; ++month)
            {
                days += month_lengths[month - 1];
                if (month == 2 && is_leap_year(time.year))
                {
                    ++days;
                }
            }
            days += time.day - 1;
            seconds = static_cast<uint32_t>(time.hour) * 3600 +
                      static_cast<uint32_t>(time.minute) * 60 + time.second;
            return true;
        }

        static int tls_tcp_write(void*, const unsigned char* buffer, size_t length)
        {
            if (!buffer || length > 0x7FFFFFFFu)
            {
                return -1;
            }

            uint32_t written = 0;
            while (written < length)
            {
                if (!client.active || client.reset || Internal::ticks() >= tls_deadline_tick)
                {
                    return -1;
                }

                uint32_t chunk_length = static_cast<uint32_t>(length - written);
                if (chunk_length > client.peer_mss)
                {
                    chunk_length = client.peer_mss;
                }
                const uint32_t segment_sequence = client.next_sequence;
                const uint32_t expected_ack = segment_sequence + chunk_length;
                if (!send_segment(0x18, segment_sequence, buffer + written, static_cast<uint16_t>(chunk_length)))
                {
                    return -1;
                }
                client.next_sequence = expected_ack;

                uint32_t attempts = 1;
                uint64_t last_send_tick = Internal::ticks();
                while (static_cast<int32_t>(client.transmit_acknowledged - expected_ack) < 0)
                {
                    if (client.reset || Internal::ticks() >= tls_deadline_tick)
                    {
                        return -1;
                    }
                    poll_network();
                    const uint64_t now = Internal::ticks();
                    if (static_cast<int32_t>(client.transmit_acknowledged - expected_ack) >= 0)
                    {
                        break;
                    }
                    if (now - last_send_tick >= 50)
                    {
                        if (attempts >= 4 ||
                            !send_segment(0x18, segment_sequence, buffer + written, static_cast<uint16_t>(chunk_length)))
                        {
                            return -1;
                        }
                        ++attempts;
                        last_send_tick = now;
                    }
                    io_wait();
                }
                written += chunk_length;
            }
            return static_cast<int>(written);
        }

        static int tls_tcp_read(void*, unsigned char* buffer, size_t length)
        {
            if (!buffer || length == 0 || length > 0x7FFFFFFFu)
            {
                return -1;
            }

            while (client.receive_length == 0)
            {
                if (client.reset || client.overflow || client.peer_closed ||
                    Internal::ticks() >= tls_deadline_tick)
                {
                    return -1;
                }
                poll_network();
                if (client.receive_length == 0)
                {
                    io_wait();
                }
            }

            uint32_t read_length = static_cast<uint32_t>(length);
            if (read_length > client.receive_length)
            {
                read_length = client.receive_length;
            }
            for (uint32_t index = 0; index < read_length; ++index)
            {
                buffer[index] = client.receive_queue[client.receive_head];
                client.receive_head = (client.receive_head + 1) % tcp_receive_queue_size;
            }
            client.receive_length -= read_length;
            return static_cast<int>(read_length);
        }
#endif
    }

    namespace Internal
    {
        void handle_tcp(const IPAddress& source, const IPAddress& destination, const uint8_t* packet, uint16_t length)
        {
            if (!client.active || !packet || length < tcp_header_size || !same_address(source, client.remote_address) || !same_address(destination, ip_addr) || Internal::read_be16(packet) != client.remote_port || Internal::read_be16(packet + 2) != client.local_port)
            {
                return;
            }

            const uint16_t header_length = static_cast<uint16_t>((packet[12] >> 4) * 4);
            if (header_length < tcp_header_size || header_length > length || tcp_checksum(source, destination, packet, length) != 0)
            {
                return;
            }

            const uint8_t flags = packet[13];
            const uint32_t sequence = Internal::read_be32(packet + 4);
            const uint32_t acknowledgement = Internal::read_be32(packet + 8);
            if (flags & 0x04)
            {
                client.reset = true;
                return;
            }

            if ((flags & 0x12) == 0x12 && !client.syn_acknowledged && acknowledgement == client.initial_sequence + 1)
            {
                client.syn_acknowledged = true;
                client.next_remote_sequence = sequence + 1;
                client.transmit_acknowledged = client.initial_sequence + 1;
                client.peer_mss = peer_mss_from_options(packet + tcp_header_size, static_cast<uint16_t>(header_length - tcp_header_size));
                send_segment(0x10, client.next_sequence);
            }
            else if ((flags & 0x12) == 0x12 && client.syn_acknowledged && acknowledgement == client.initial_sequence + 1)
            {
                send_segment(0x10, client.next_sequence);
                return;
            }
            else if (!client.syn_acknowledged)
            {
                return;
            }

            if ((flags & 0x10) && client.syn_acknowledged)
            {
                if (static_cast<int32_t>(acknowledgement - client.transmit_acknowledged) > 0 &&
                    static_cast<int32_t>(client.next_sequence - acknowledgement) >= 0)
                {
                    client.transmit_acknowledged = acknowledgement;
                }
                if (client.request_sent && acknowledgement == client.next_sequence)
                {
                    client.request_acknowledged = true;
                }
            }

            const uint16_t payload_length = static_cast<uint16_t>(length - header_length);
            uint32_t payload_sequence = sequence + ((flags & 0x02) ? 1u : 0u);
            if (payload_length > 0)
            {
                if (payload_sequence == client.next_remote_sequence)
                {
                    uint32_t copy_length = payload_length;
                    if (client.stream_mode)
                    {
                        const uint32_t available = tcp_receive_queue_size - client.receive_length;
                        if (copy_length > available)
                        {
                            copy_length = available;
                        }
                        for (uint32_t index = 0; index < copy_length; ++index)
                        {
                            const uint32_t tail = (client.receive_head + client.receive_length + index) % tcp_receive_queue_size;
                            client.receive_queue[tail] = packet[header_length + index];
                        }
                        client.receive_length += copy_length;
                    }
                    else
                    {
                        const uint32_t available = client.response_capacity - 1 - client.response_length;
                        if (copy_length > available)
                        {
                            copy_length = available;
                        }
                        for (uint32_t index = 0; index < copy_length; ++index)
                        {
                            client.response[client.response_length + index] = static_cast<char>(packet[header_length + index]);
                        }
                        client.response_length += copy_length;
                        client.response[client.response_length] = '\0';
                    }
                    if (copy_length < payload_length)
                    {
                        client.overflow = true;
                    }
                    client.next_remote_sequence += payload_length;
                }
                send_segment(0x10, client.next_sequence);
                payload_sequence += payload_length;
            }

            if (flags & 0x01)
            {
                if (payload_sequence == client.next_remote_sequence)
                {
                    ++client.next_remote_sequence;
                    client.peer_closed = true;
                    send_segment(0x10, client.next_sequence);
                    if (!client.local_fin_sent)
                    {
                        if (send_segment(0x11, client.next_sequence))
                        {
                            ++client.next_sequence;
                            client.local_fin_sent = true;
                        }
                    }
                }
                else
                {
                    send_segment(0x10, client.next_sequence);
                }
            }
        }
    }

    int32_t http_get(const char* hostname, uint16_t port, const char* path, char* response, uint32_t response_capacity, uint32_t timeout_ms)
    {
#if !defined(__x86_64__)
        (void)hostname;
        (void)port;
        (void)path;
        (void)response;
        (void)response_capacity;
        (void)timeout_ms;
        return -1;
#else
        if (!available || ip_addr.bytes[0] == 0)
        {
            return -1;
        }
        if (!hostname || !*hostname || !port || !valid_http_text(hostname, false) || !valid_http_text(path, true) || !response || response_capacity < 2 || response_capacity > 0x7FFFFFFFu)
        {
            return -6;
        }
        if (client.active)
        {
            return -1;
        }

        uint32_t effective_timeout = timeout_ms == 0 ? 15000 : timeout_ms;
        if (effective_timeout > 30000)
        {
            effective_timeout = 30000;
        }

        IPAddress remote_address = {};
        if (!parse_ipv4_address(hostname, remote_address) && !resolve_hostname(hostname, remote_address, effective_timeout < 3000 ? effective_timeout : 3000))
        {
            return -2;
        }

        char request[1024] = {};
        uint32_t request_length = 0;
        if (!append_request(request, sizeof(request), request_length, "GET ") || !append_request(request, sizeof(request), request_length, path) || !append_request(request, sizeof(request), request_length, " HTTP/1.0\r\nHost: ") || !append_request(request, sizeof(request), request_length, hostname))
        {
            return -6;
        }
        if (port != 80)
        {
            char port_text[6] = {};
            uint16_t value = port;
            int digits = 0;
            do
            {
                port_text[digits++] = static_cast<char>('0' + value % 10);
                value = static_cast<uint16_t>(value / 10);
            }
            while (value != 0 && digits < 5);
            for (int left = 0, right = digits - 1; left < right; ++left, --right)
            {
                const char swap = port_text[left];
                port_text[left] = port_text[right];
                port_text[right] = swap;
            }
            port_text[digits] = '\0';
            if (!append_request(request, sizeof(request), request_length, ":") || !append_request(request, sizeof(request), request_length, port_text))
            {
                return -6;
            }
        }
        if (!append_request(request, sizeof(request), request_length, "\r\nUser-Agent: NasuaOS/1.0\r\nAccept: text/html,*/*\r\nConnection: close\r\n\r\n") || request_length > tcp_max_payload)
        {
            return -6;
        }

        client = {};
        client.active = true;
        client.remote_address = remote_address;
        client.remote_port = port;
        client.local_port = next_local_port++;
        if (next_local_port < tcp_local_port_start)
        {
            next_local_port = tcp_local_port_start;
        }
        client.peer_mss = tcp_max_payload;
        client.response = reinterpret_cast<uint8_t*>(response);
        client.response_capacity = response_capacity;
        client.initial_sequence = static_cast<uint32_t>(Internal::ticks() * 1103515245u) ^ (static_cast<uint32_t>(client.local_port) << 16) ^ Internal::read_be32(ip_addr.bytes);
        client.next_sequence = client.initial_sequence;
        client.request_sequence = client.initial_sequence + 1;
        response[0] = '\0';

        const uint64_t timeout_ticks = (effective_timeout + 9) / 10;
        const uint64_t start_tick = Internal::ticks();
        uint64_t last_syn_tick = start_tick;
        uint64_t last_request_tick = 0;
        uint32_t syn_attempts = 0;
        uint32_t request_attempts = 0;
        bool request_sent = false;

        while (!client.reset && Internal::ticks() - start_tick < timeout_ticks)
        {
            poll_network();
            const uint64_t now = Internal::ticks();
            if (!client.syn_acknowledged)
            {
                if (syn_attempts == 0 || now - last_syn_tick >= 100)
                {
                    if (syn_attempts >= 4 ||
                        !send_segment(0x02, client.initial_sequence))
                    {
                        client.reset = true;
                        break;
                    }
                    if (syn_attempts == 0)
                    {
                        client.next_sequence = client.initial_sequence + 1;
                    }
                    ++syn_attempts;
                    last_syn_tick = now;
                }
            }
            else if (!request_sent)
            {
                if (request_length > client.peer_mss)
                {
                    client.reset = true;
                    break;
                }
                client.next_sequence = client.request_sequence;
                if (!send_segment(0x18, client.request_sequence, reinterpret_cast<const uint8_t*>(request), static_cast<uint16_t>(request_length)))
                {
                    client.reset = true;
                    break;
                }
                client.next_sequence += request_length;
                request_sent = true;
                client.request_sent = true;
                ++request_attempts;
                last_request_tick = now;
            }
            else if (!client.request_acknowledged && request_attempts < 4 && now - last_request_tick >= 100)
            {
                if (!send_segment(0x18, client.request_sequence, reinterpret_cast<const uint8_t*>(request), static_cast<uint16_t>(request_length)))
                {
                    client.reset = true;
                    break;
                }
                ++request_attempts;
                last_request_tick = now;
            }

            if (client.peer_closed)
            {
                break;
            }
            io_wait();
        }

        const bool timed_out = Internal::ticks() - start_tick >= timeout_ticks;
        const bool connected = client.syn_acknowledged;
        const bool overflow = client.overflow;
        const uint32_t response_length = client.response_length;
        if (connected && !client.local_fin_sent)
        {
            if (send_segment(0x11, client.next_sequence))
            {
                ++client.next_sequence;
            }
        }
        client.active = false;

        if (overflow)
        {
            return -5;
        }
        if (timed_out)
        {
            return -4;
        }
        if (client.reset || !connected || response_length == 0)
        {
            return connected ? -4 : -3;
        }
        return static_cast<int32_t>(response_length);
#endif
    }

    int32_t https_get(const char* hostname, uint16_t port, const char* path,
                      char* response, uint32_t response_capacity, uint32_t timeout_ms)
    {
#if !defined(__x86_64__)
        (void)hostname;
        (void)port;
        (void)path;
        (void)response;
        (void)response_capacity;
        (void)timeout_ms;
        return -1;
#else
        if (!available || ip_addr.bytes[0] == 0)
        {
            return -1;
        }
        if (!hostname || !*hostname || !port || !valid_http_text(hostname, false) ||
            !valid_http_text(path, true) || !response || response_capacity < 2 ||
            response_capacity > 0x7FFFFFFFu)
        {
            return -6;
        }
        if (client.active)
        {
            return -1;
        }

        uint32_t certificate_days = 0;
        uint32_t certificate_seconds = 0;
        if (!get_certificate_time(certificate_days, certificate_seconds))
        {
            return -9;
        }
        uint8_t entropy[32];
        if (!secure_random_bytes(entropy, sizeof(entropy)))
        {
            return -10;
        }

        uint32_t effective_timeout = timeout_ms == 0 ? 15000 : timeout_ms;
        if (effective_timeout > 30000)
        {
            effective_timeout = 30000;
        }

        IPAddress remote_address = {};
        if (!parse_ipv4_address(hostname, remote_address) &&
            !resolve_hostname(hostname, remote_address,
                              effective_timeout < 3000 ? effective_timeout : 3000))
        {
            return -2;
        }

        br_ssl_client_init_full(&tls_client, &tls_x509, TAs, nasua_mozilla_trust_anchor_count);
        br_ssl_engine_set_versions(&tls_client.eng, BR_TLS12, BR_TLS12);
        br_ssl_engine_set_buffer(&tls_client.eng, tls_buffer, sizeof(tls_buffer), 1);
        br_x509_minimal_set_time(&tls_x509, certificate_days, certificate_seconds);
        br_ssl_engine_inject_entropy(&tls_client.eng, entropy, sizeof(entropy));
        for (uint32_t index = 0; index < sizeof(entropy); ++index)
        {
            entropy[index] = 0;
        }

        client = {};
        client.active = true;
        client.stream_mode = true;
        client.remote_address = remote_address;
        client.remote_port = port;
        client.local_port = next_local_port++;
        if (next_local_port < tcp_local_port_start)
        {
            next_local_port = tcp_local_port_start;
        }
        client.peer_mss = tcp_max_payload;
        client.initial_sequence = static_cast<uint32_t>(Internal::ticks() * 1103515245u) ^
                                  (static_cast<uint32_t>(client.local_port) << 16) ^
                                  Internal::read_be32(ip_addr.bytes);
        client.next_sequence = client.initial_sequence;
        client.request_sequence = client.initial_sequence + 1;
        response[0] = '\0';

        const uint64_t timeout_ticks = (effective_timeout + 9) / 10;
        const uint64_t start_tick = Internal::ticks();
        tls_deadline_tick = start_tick + timeout_ticks;
        uint64_t last_syn_tick = start_tick;
        uint32_t syn_attempts = 0;
        while (!client.reset && !client.syn_acknowledged &&
               Internal::ticks() - start_tick < timeout_ticks)
        {
            poll_network();
            const uint64_t now = Internal::ticks();
            if (syn_attempts == 0 || now - last_syn_tick >= 100)
            {
                if (syn_attempts >= 4 || !send_segment(0x02, client.initial_sequence))
                {
                    client.reset = true;
                    break;
                }
                if (syn_attempts == 0)
                {
                    client.next_sequence = client.initial_sequence + 1;
                }
                ++syn_attempts;
                last_syn_tick = now;
            }
            io_wait();
        }

        if (!client.syn_acknowledged)
        {
            const bool timed_out = Internal::ticks() - start_tick >= timeout_ticks;
            client.active = false;
            return timed_out ? -4 : -3;
        }

        char request[1024] = {};
        uint32_t request_length = 0;
        if (!append_request(request, sizeof(request), request_length, "GET ") ||
            !append_request(request, sizeof(request), request_length, path) ||
            !append_request(request, sizeof(request), request_length, " HTTP/1.0\r\nHost: ") ||
            !append_request(request, sizeof(request), request_length, hostname))
        {
            client.active = false;
            return -6;
        }
        if (port != 443)
        {
            char port_text[6] = {};
            uint16_t value = port;
            int digits = 0;
            do
            {
                port_text[digits++] = static_cast<char>('0' + value % 10);
                value = static_cast<uint16_t>(value / 10);
            }
            while (value != 0 && digits < 5);
            for (int left = 0, right = digits - 1; left < right; ++left, --right)
            {
                const char swap = port_text[left];
                port_text[left] = port_text[right];
                port_text[right] = swap;
            }
            port_text[digits] = '\0';
            if (!append_request(request, sizeof(request), request_length, ":") ||
                !append_request(request, sizeof(request), request_length, port_text))
            {
                client.active = false;
                return -6;
            }
        }
        if (!append_request(request, sizeof(request), request_length,
                            "\r\nUser-Agent: NasuaOS/1.0\r\nAccept: text/html,*/*\r\nConnection: close\r\n\r\n"))
        {
            client.active = false;
            return -6;
        }

        if (!br_ssl_client_reset(&tls_client, hostname, 0))
        {
            client.active = false;
            return -7;
        }
        br_sslio_init(&tls_io, &tls_client.eng, tls_tcp_read, nullptr, tls_tcp_write, nullptr);

        int32_t result = -7;
        if (br_sslio_write_all(&tls_io, request, request_length) == 0 &&
            br_sslio_flush(&tls_io) == 0)
        {
            uint32_t response_length = 0;
            for (;;)
            {
                const uint32_t available_length = response_capacity - 1 - response_length;
                if (available_length == 0)
                {
                    uint8_t extra_byte = 0;
                    const int extra_length = br_sslio_read(&tls_io, &extra_byte, 1);
                    if (extra_length > 0)
                    {
                        result = -5;
                        break;
                    }
                    const unsigned error = br_ssl_engine_last_error(&tls_client.eng);
                    result = error == BR_ERR_OK ? static_cast<int32_t>(response_length) :
                             (error >= BR_ERR_X509_INVALID_VALUE && error <= BR_ERR_X509_NOT_TRUSTED ? -8 : -7);
                    break;
                }

                uint32_t chunk_length = available_length;
                if (chunk_length > sizeof(tls_read_buffer))
                {
                    chunk_length = sizeof(tls_read_buffer);
                }
                const int read_length = br_sslio_read(&tls_io, tls_read_buffer, chunk_length);
                if (read_length <= 0)
                {
                    const unsigned error = br_ssl_engine_last_error(&tls_client.eng);
                    if (error == BR_ERR_OK)
                    {
                        result = static_cast<int32_t>(response_length);
                    }
                    else if (error >= BR_ERR_X509_INVALID_VALUE && error <= BR_ERR_X509_NOT_TRUSTED)
                    {
                        result = -8;
                    }
                    else if (Internal::ticks() >= tls_deadline_tick)
                    {
                        result = -4;
                    }
                    else
                    {
                        result = -7;
                    }
                    break;
                }

                for (int index = 0; index < read_length; ++index)
                {
                    response[response_length + static_cast<uint32_t>(index)] =
                        static_cast<char>(tls_read_buffer[index]);
                }
                response_length += static_cast<uint32_t>(read_length);
                response[response_length] = '\0';
            }
        }
        else
        {
            const unsigned error = br_ssl_engine_last_error(&tls_client.eng);
            if (error >= BR_ERR_X509_INVALID_VALUE && error <= BR_ERR_X509_NOT_TRUSTED)
            {
                result = -8;
            }
            else if (Internal::ticks() >= tls_deadline_tick)
            {
                result = -4;
            }
        }

        if (client.syn_acknowledged && !client.local_fin_sent)
        {
            if (send_segment(0x11, client.next_sequence))
            {
                ++client.next_sequence;
                client.local_fin_sent = true;
            }
        }
        client.active = false;
        return result;
#endif
    }
}