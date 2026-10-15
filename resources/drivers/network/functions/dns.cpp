#include "network_internal.hpp"

#include "libs/asm/asm.hpp"

namespace Network
{
	namespace
	{
		constexpr uint16_t dns_client_port = 53000;
		uint16_t next_dns_id = 0x4E53;
		uint16_t active_dns_id = 0;
		bool dns_query_complete = false;
		bool dns_query_succeeded = false;
		IPAddress dns_query_address = {};

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

		bool skip_name(const uint8_t* packet, uint16_t length, uint16_t& offset)
		{
			while (offset < length)
			{
				const uint8_t label_length = packet[offset++];
				if (label_length == 0)
				{
					return true;
				}
				if ((label_length & 0xC0) == 0xC0)
				{
					if (offset >= length)
					{
						return false;
					}
					const uint16_t pointer = static_cast<uint16_t>(((label_length & 0x3F) << 8) | packet[offset]);
					++offset;
					return pointer < length;
				}
				if ((label_length & 0xC0) != 0 || label_length > length - offset)
				{
					return false;
				}
				offset = static_cast<uint16_t>(offset + label_length);
			}
			return false;
		}

		void receive_dns_response(const IPAddress& source, uint16_t source_port, const uint8_t* packet, uint16_t length)
		{
			if (!packet || source_port != 53 || !same_address(source, dns_server) || length < 12 ||
				Internal::read_be16(packet) != active_dns_id)
			{
				return;
			}

			const uint16_t flags = Internal::read_be16(packet + 2);
			const uint16_t question_count = Internal::read_be16(packet + 4);
			const uint16_t answer_count = Internal::read_be16(packet + 6);
			if ((flags & 0x8000) == 0 || (flags & 0x7800) != 0 || question_count == 0)
			{
				return;
			}

			uint16_t offset = 12;
			if (!skip_name(packet, length, offset) || length - offset < 4 || Internal::read_be16(packet + offset) != 1 || Internal::read_be16(packet + offset + 2) != 1)
			{
				return;
			}
			offset = static_cast<uint16_t>(offset + 4);
			if ((flags & 0x0200) != 0 || (flags & 0x000F) != 0)
			{
				dns_query_complete = true;
				return;
			}

			for (uint16_t answer = 0; answer < answer_count; ++answer)
			{
				if (!skip_name(packet, length, offset) || length - offset < 10)
				{
					return;
				}
				const uint16_t type = Internal::read_be16(packet + offset);
				const uint16_t record_class = Internal::read_be16(packet + offset + 2);
				const uint16_t data_length = Internal::read_be16(packet + offset + 8);
				offset = static_cast<uint16_t>(offset + 10);
				if (data_length > length - offset)
				{
					return;
				}
				if (type == 1 && record_class == 1 && data_length == 4)
				{
					for (uint8_t index = 0; index < 4; ++index)
					{
						dns_query_address.bytes[index] = packet[offset + index];
					}
					dns_query_succeeded = true;
					dns_query_complete = true;
					return;
				}
				offset = static_cast<uint16_t>(offset + data_length);
			}
			dns_query_complete = true;
		}

		bool create_query(const char* hostname, uint8_t* packet, uint16_t& length)
		{
			if (!hostname || !*hostname)
			{
				return false;
			}

			uint16_t offset = 12;
			uint16_t name_length = 0;
			const char* cursor = hostname;
			while (*cursor)
			{
				if (*cursor == '.')
				{
					return false;
				}
				const uint16_t label_offset = offset++;
				uint8_t label_length = 0;
				char first_character = 0;
				char last_character = 0;
				while (*cursor && *cursor != '.')
				{
					const char character = *cursor++;
					const bool valid_character = (character >= 'a' && character <= 'z') ||
												 (character >= 'A' && character <= 'Z') ||
												 (character >= '0' && character <= '9') || character == '-';
					if (!valid_character || label_length == 63)
					{
						return false;
					}
					if (label_length == 0)
					{
						first_character = character;
					}
					last_character = character;
					packet[offset++] = static_cast<uint8_t>(character);
					++label_length;
					++name_length;
				}
				if (label_length == 0 || first_character == '-' || last_character == '-')
				{
					return false;
				}
				packet[label_offset] = label_length;
				if (*cursor == '.')
				{
					++cursor;
					if (*cursor)
					{
						++name_length;
						if (name_length > 253)
						{
							return false;
						}
					}
				}
				if (name_length > 253)
				{
					return false;
				}
			}

			packet[offset++] = 0;
			Internal::write_be16(packet + offset, 1);
			Internal::write_be16(packet + offset + 2, 1);
			length = static_cast<uint16_t>(offset + 4);
			return true;
		}
	}

	bool resolve_hostname(const char* hostname, IPAddress& address, uint32_t timeout_ms)
	{
		if (!available || !hostname || dns_server.bytes[0] == 0)
		{
			return false;
		}

		uint8_t packet[512] = {};
		uint16_t packet_length = 0;
		if (!create_query(hostname, packet, packet_length))
		{
			return false;
		}

		active_dns_id = ++next_dns_id;
		if (active_dns_id == 0)
		{
			active_dns_id = ++next_dns_id;
		}
		Internal::write_be16(packet, active_dns_id);
		Internal::write_be16(packet + 2, 0x0100);
		Internal::write_be16(packet + 4, 1);

		dns_query_complete = false;
		dns_query_succeeded = false;
		dns_query_address = {};
		if (!bind_udp_port(dns_client_port, receive_dns_response))
		{
			return false;
		}

		const bool sent = send_udp_packet(dns_server, dns_client_port, 53, packet, packet_length);
		const uint64_t timeout_ticks = (static_cast<uint64_t>(timeout_ms) + 9) / 10;
		const uint64_t start = Internal::ticks();
		while (sent && !dns_query_complete && Internal::ticks() - start < timeout_ticks)
		{
			poll();
			io_wait();
		}
		unbind_udp_port(dns_client_port);

		if (!dns_query_complete || !dns_query_succeeded)
		{
			return false;
		}
		address = dns_query_address;
		return true;
	}
}
