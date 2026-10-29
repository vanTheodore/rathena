// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
// Contracts for the actual C++23 encoder used by login-server.
#include <common/packet_bytes.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

using rathena::packet_bytes::encode_auth_hash;
using rathena::packet_bytes::encode_error;

int main() {
	int failures = 0;
	const auto check = [&failures](bool ok, const char* name) {
		if (!ok) {
			std::printf("FAIL: %s\n", name);
			++failures;
		}
	};

	std::array<std::byte, 32> output{};
	output.fill(std::byte{0xa5});
	const std::array<std::byte, 4> salt{
		std::byte{0x12}, std::byte{0}, std::byte{0xff}, std::byte{0x34}
	};
	auto frame = encode_auth_hash(output, salt, 0x01dc);
	constexpr std::array<std::byte, 8> expected{
		std::byte{0xdc}, std::byte{0x01}, std::byte{0x08}, std::byte{0x00},
		std::byte{0x12}, std::byte{0x00}, std::byte{0xff}, std::byte{0x34}
	};
	check(frame.has_value(), "normal packet encoded");
	if (frame) {
		check(frame->size() == expected.size(), "exact packet length");
		check(std::ranges::equal(*frame, expected), "wire bytes including embedded NUL");
	}
	check(std::ranges::all_of(output.begin() + expected.size(), output.end(),
		[](std::byte b) { return b == std::byte{0xa5}; }), "unused capacity untouched");

	output.fill(std::byte{0xa5});
	const std::array<std::byte, 0> empty_salt{};
	auto empty = encode_auth_hash(output, empty_salt, 0x01dc);
	check(empty && empty->size() == 4, "zero salt yields a 4-byte packet");
	check(output[2] == std::byte{0x04} && output[3] == std::byte{0x00}, "zero salt length bytes");

	std::array<std::byte, 7> short_buffer{};
	short_buffer.fill(std::byte{0xa5});
	const auto short_result = encode_auth_hash(short_buffer, salt, 0x01dc);
	check(!short_result && short_result.error() == encode_error::buffer_too_small,
		"reject too-small destination");
	check(std::ranges::all_of(short_buffer, [](std::byte b) { return b == std::byte{0xa5}; }),
		"short destination not written");

	std::vector<std::byte> max_payload(65535 - 4, std::byte{0x5e});
	std::vector<std::byte> full(65535, std::byte{0});
	auto max_frame = encode_auth_hash(full, max_payload, 0x01dc);
	check(max_frame && max_frame->size() == 65535, "maximum 16-bit wire length");
	check(full[2] == std::byte{0xff} && full[3] == std::byte{0xff}, "maximum length encoded LE");
	check(full[4] == std::byte{0x5e} && full.back() == std::byte{0x5e}, "max payload boundaries");

	max_payload.push_back(std::byte{0xff});
	full.assign(65536, std::byte{0xa5});
	const auto overflow = encode_auth_hash(full, max_payload, 0x01dc);
	check(!overflow && overflow.error() == encode_error::payload_too_large,
		"reject length overflow");
	check(std::ranges::all_of(full, [](std::byte b) { return b == std::byte{0xa5}; }),
		"oversize payload not written");

	if (failures == 0)
		std::puts("PACKET BYTES C++23 CONTRACT PASSED");
	return failures != 0;
}
