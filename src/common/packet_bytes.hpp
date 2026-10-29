// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef RATHENA_PACKET_BYTES_HPP
#define RATHENA_PACKET_BYTES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>

namespace rathena::packet_bytes {

enum class encode_error {
	payload_too_large,
	buffer_too_small,
};

// Auth-key reply: [opcode: uint16 LE][length: uint16 LE][salt: raw bytes].
// Return a view of the initialized bytes; never expose capacity as packet
// length. A std::span makes the storage bound explicit, unlike a cast to a
// flexible-array packet struct. The payload is binary (NULs are valid).
[[nodiscard]] inline std::expected<std::span<const std::byte>, encode_error>
encode_auth_hash(std::span<std::byte> output, std::span<const std::byte> salt,
                 std::uint16_t opcode) noexcept {
	constexpr std::size_t header_size = 2 * sizeof(std::uint16_t);
	constexpr auto max_size = std::numeric_limits<std::uint16_t>::max();
	if (salt.size() > max_size - header_size)
		return std::unexpected(encode_error::payload_too_large);

	const std::size_t length = header_size + salt.size();
	if (output.size() < length)
		return std::unexpected(encode_error::buffer_too_small);

	output[0] = static_cast<std::byte>(opcode & 0xff);
	output[1] = static_cast<std::byte>(opcode >> 8);
	output[2] = static_cast<std::byte>(length & 0xff);
	output[3] = static_cast<std::byte>(length >> 8);
	std::ranges::copy(salt, output.begin() + header_size);
	return std::span<const std::byte>(output.data(), length);
}

} // namespace rathena::packet_bytes

#endif // RATHENA_PACKET_BYTES_HPP
