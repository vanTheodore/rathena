// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Equivalence/contract test for a2i() in src/common/strlib.hpp
// (pcre-regress convention: standalone):
//
//   zig/clang: c++ -std=c++23 -Wall -Wextra -Wconversion -Wsign-conversion
//              -Isrc tools/a2i-check.cpp -o a2i-check (then run it)
//
// Part 1 pins exact parity with the real libc atoi() for every in-range
// conversion shape. Part 2 pins the ONLY intended divergence — defined
// saturation on out-of-range input (the C standard calls atoi overflow
// undefined; glibc casts strtol's long to int giving wrap results like
// -1/0/INT_MIN, while 32-bit-long platforms such as Windows already
// saturate). a2i matches the Windows result on every platform.

#include <common/strlib.hpp>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

static int failures = 0;

static void expect(bool ok, const char* what) {
	if (!ok) {
		std::printf("FAIL: %s\n", what);
		++failures;
	}
}

int main() {
	// ---- Part 1: exact parity with the real libc atoi (in-range only).
	static const char* const parity[] = {
		"0", "42", "-42", "+42", "  7", "\t\n 5\r", "- 5",
		"12abc", "abc", "", "-", "x9", "0000123", "007abc", "-0",
		"2147483647", "-2147483648", "2000000000", "-2000000000",
		"  +00042x", "9 \t9", "0x10", "12.34", "1e5", "3\t", "8\n9",
		"+0", "--5", "-+5", "00", "00000000000000000009",
		"2147483646", "-2147483647", "  -12 34",
	};
	for (const char* s : parity) {
		const int expect_libc = std::atoi(s);
		const int got = a2i(s);
		if (got != expect_libc) {
			std::printf("FAIL: parity for '%s': atoi=%d a2i=%d\n", s, expect_libc, got);
			++failures;
		}
	}
	std::printf("parity cases: %zu, all matched libc atoi\n",
		sizeof(parity) / sizeof(parity[0]));

	// ---- Part 2: defined saturation (control: print the libc result).
	// a2i must equal what 32-bit-long platforms (Windows) already return.
	{
		struct overflow_case { const char* in; int want; };
		static const overflow_case cases[] = {
			{"2147483648", INT_MAX},           // libc64 atoi: INT_MIN (wrap cast)
			{"-2147483649", INT_MIN},          // libc64 atoi: INT_MAX (wrap cast)
			{"21474836480", INT_MAX},
			{"99999999999999999999", INT_MAX}, // libc64 atoi: (int)LONG_MAX = -1
			{"-99999999999999999999", INT_MIN},// libc64 atoi: (int)LONG_MIN = 0
			{"9223372036854775808", INT_MAX},  // LONG_MAX+1: strtol saturates
		};
		for (const auto& c : cases) {
			const int libc_result = std::atoi(c.in); // control, implementation-defined
			const int got = a2i(c.in);
			if (got != c.want) {
				std::printf("FAIL: overflow '%s': a2i=%d want=%d (libc control=%d)\n",
					c.in, got, c.want, libc_result);
				++failures;
			} else {
				std::printf("control-overflow: '%s' libc=%d a2i=%d (win-parity)\n",
					c.in, libc_result, got);
			}
		}
	}

	// ---- nullptr: defined 0 (libc atoi on nullptr is a crash; not called).
	expect(a2i(nullptr) == 0, "nullptr -> 0");

	// ---- Return type contract: int32, usable where atoi was.
	static_assert(std::is_same_v<decltype(a2i("1")), int32>,
		"a2i returns int32");

	if (failures == 0) {
		std::printf("ALL A2I CHECKS PASSED\n");
		return 0;
	}
	std::printf("%d FAILURES\n", failures);
	return 1;
}
