// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Contracts for the REAL C++23 byte/word helpers used by src/common/utils.cpp.
// C++20: bit_cast, endian, span, concepts; C++23: byteswap.
#include <common/byteorder.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

using namespace rathena::byteorder;

constexpr std::array<unsigned char, 2> two{0x34, 0x12};
constexpr std::array<unsigned char, 4> four{0x78, 0x56, 0x34, 0x12};
constexpr std::array<unsigned char, 4> negative{0xff, 0xff, 0xff, 0xff};
constexpr std::array<unsigned char, 4> float_1{0x00, 0x00, 0x80, 0x3f};
constexpr std::array<unsigned char, 4> neg_zero{0x00, 0x00, 0x00, 0x80};

static_assert(load_u16_le(two) == 0x1234u);
static_assert(load_u32_le(four) == 0x12345678u);
static_assert(load_i32_le(negative) == -1);
static_assert(load_f32_le(float_1) == 1.0f);
static_assert(std::bit_cast<std::uint32_t>(load_f32_le(neg_zero)) == 0x80000000u);
static_assert(combine_words(0x5678u, 0x1234u) == 0x12345678u);
static_assert(combine_words(0xffffu, 0xffffu) == 0xffffffffu);
static_assert(std::byteswap(std::uint32_t{0x12345678u}) == 0x78563412u);
static_assert(native_to_little_endian(std::uint32_t{0x12345678u}) ==
	(std::endian::native == std::endian::little ? 0x12345678u : 0x78563412u));
static_assert(native_to_little_endian(std::int16_t{-1}) == -1);

int main() {
	// Numeric comparisons cannot detect NaN payload and negative-zero changes.
	constexpr std::array<unsigned char, 4> nan_bytes{0x34, 0x12, 0xc0, 0x7f};
	if (std::bit_cast<std::uint32_t>(load_f32_le(nan_bytes)) != 0x7fc01234u) {
		std::puts("FAILED: NaN payload not preserved");
		return 1;
	}
	if (!std::numeric_limits<float>::is_iec559 || sizeof(float) != 4) {
		std::puts("FAILED: platform float representation no longer matches packet format");
		return 1;
	}
	std::puts("BYTEORDER C++23 CHECK PASSED");
	return 0;
}
