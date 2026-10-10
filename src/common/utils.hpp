// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio> // FILE*
#include <type_traits> // common_type_t

#include "cbasetypes.hpp"

// generate a hex dump of the first 'length' bytes of 'buffer'
void WriteDump(FILE* fp, const void* buffer, size_t length);
void ShowDump(const void* buffer, size_t length);

int32 check_filepath(const char* filepath);
void findfile(const char *p, const char *pat, void (func)(const char*));
bool exists(const char* filename);

/// Caps values to min/max
#define cap_value(a, min, max) (((a) >= (max)) ? (max) : ((a) <= (min)) ? (min) : (a))

/// Apply rate for val, divided by 100)
/// Function template (not a macro): each argument is evaluated exactly once,
/// and mismatched argument types resolve through std::common_type exactly as
/// the macro's usual arithmetic conversions did — with the conversions made
/// explicit so no new -Wconversion/-Wsign-conversion diagnostics appear.
template <typename T, typename R>
constexpr auto apply_rate(T val, R rate)
{
	using C = std::common_type_t<T, R>;
	const auto v = static_cast<C>(val);
	const auto r = static_cast<C>(rate);
	return (r == 100) ? v : (v > 100000) ? (v / 100 * r) : (v * r / 100);
}

/// Apply rate for val, divided by per
/// Same single-evaluation and conversion guarantees as apply_rate.
template <typename T, typename R, typename P>
constexpr auto apply_rate2(T val, R rate, P per)
{
	using C = std::common_type_t<T, R, P>;
	const auto v = static_cast<C>(val);
	const auto r = static_cast<C>(rate);
	const auto p = static_cast<C>(per);
	return (r == p) ? v : (v > 100000) ? (v / p * r) : (v * r / p);
}

/// calculates the value of A / B, in percent (rounded down)
uint32 get_percentage(const uint32 A, const uint32 B);
uint32 get_percentage_exp(const uint64 a, const uint64 b);

//////////////////////////////////////////////////////////////////////////
// byte word dword access [Shinomori]
//////////////////////////////////////////////////////////////////////////

extern uint8 GetByte(uint32 val, int32 idx);
extern uint16 GetWord(uint32 val, int32 idx);
extern uint16 MakeWord(uint8 byte0, uint8 byte1);
extern uint32 MakeDWord(uint16 word0, uint16 word1);

//////////////////////////////////////////////////////////////////////////
// Big-endian compatibility functions
//////////////////////////////////////////////////////////////////////////
extern int16 MakeShortLE(int16 val);
extern int32 MakeLongLE(int32 val);
extern uint16 GetUShort(const unsigned char* buf);
extern uint32 GetULong(const unsigned char* buf);
extern int32 GetLong(const unsigned char* buf);
extern float GetFloat(const unsigned char* buf);

#endif /* UTILS_HPP */
