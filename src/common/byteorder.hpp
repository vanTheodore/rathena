// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef RATHENA_BYTEORDER_HPP
#define RATHENA_BYTEORDER_HPP

#include <bit>
#include <concepts>
#include <cstdint>
#include <span>

namespace rathena::byteorder {

// Convert an integral value's native representation into little-endian bytes.
// Replaces assembling bytes on the stack and type-punning through int16/int32*.
template <std::integral T>
  requires (sizeof(T) == 2 || sizeof(T) == 4)
[[nodiscard]] constexpr T native_to_little_endian(T value) noexcept {
	if constexpr (std::endian::native == std::endian::big) {
		return std::byteswap(value); // C++23, works for signed integers too.
	} else {
		static_assert(std::endian::native == std::endian::little,
			"rAthena supports only pure little- or big-endian targets");
		return value;
	}
}

// Fixed-extent spans document the pre-existing contract: the pointer passed
// by the public Get* functions must refer to at least 2 or 4 readable bytes.
[[nodiscard]] constexpr std::uint16_t load_u16_le(std::span<const unsigned char, 2> bytes) noexcept {
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(bytes[0])
		| (static_cast<std::uint16_t>(bytes[1]) << 8));
}

[[nodiscard]] constexpr std::uint32_t load_u32_le(std::span<const unsigned char, 4> bytes) noexcept {
	return static_cast<std::uint32_t>(bytes[0])
		| (static_cast<std::uint32_t>(bytes[1]) << 8)
		| (static_cast<std::uint32_t>(bytes[2]) << 16)
		| (static_cast<std::uint32_t>(bytes[3]) << 24);
}

[[nodiscard]] constexpr std::int32_t load_i32_le(std::span<const unsigned char, 4> bytes) noexcept {
	return std::bit_cast<std::int32_t>(load_u32_le(bytes));
}

[[nodiscard]] constexpr float load_f32_le(std::span<const unsigned char, 4> bytes) noexcept {
	return std::bit_cast<float>(load_u32_le(bytes));
}

[[nodiscard]] constexpr std::uint32_t combine_words(std::uint16_t low, std::uint16_t high) noexcept {
	return static_cast<std::uint32_t>(low)
		| (static_cast<std::uint32_t>(high) << 16);
}

} // namespace rathena::byteorder

#endif // RATHENA_BYTEORDER_HPP
