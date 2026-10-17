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
#include <stdexcept>
#include <string>
#include <type_traits>

static int failures = 0;
static int stoi_threw = 0; // inputs where std::stoi would have terminated

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

	// ---- Part 3: std::stoi migration parity (the 167 replaced sites).
	// Where std::stoi produced a value it must match; where std::stoi
	// THREW (invalid_argument on no-conversion: "", "abc", "- 5", ...)
	// — the uncaught-terminate cases the migration removed — a2i must
	// keep atoi's documented 0.
	for (const char* s : parity) {
		const std::string as_string(s);
		int stoi_result = 0;
		bool stoi_ok = true;
		try {
			stoi_result = std::stoi(as_string);
		} catch (const std::exception&) {
			stoi_ok = false;
		}
		if (stoi_ok) {
			if (a2i(as_string) != stoi_result || a2i(s) != stoi_result) {
				std::printf("FAIL: stoi parity for '%s': stoi=%d a2i=%d\n",
					s, stoi_result, a2i(s));
				++failures;
			}
		} else {
			if (a2i(as_string) != 0 || a2i(s) != 0) {
				std::printf("FAIL: stoi threw for '%s' but a2i=%d (want 0)\n",
					s, a2i(s));
				++failures;
			}
			++stoi_threw;
		}
	}
	std::printf("stoi-parity cases: %zu (%d were throw-and-terminate class)\n",
		sizeof(parity) / sizeof(parity[0]), stoi_threw);

	// ---- Part 4: the crash classes the migration removed. std::stoi
	// THROWS on these and no translation unit contained a catch —
	// uncaught => std::terminate. a2i returns a defined value instead.
	{
		bool threw_invalid = false;
		try {
			(void)std::stoi(std::string("abc"));
		} catch (const std::invalid_argument&) {
			threw_invalid = true;
		}
		expect(threw_invalid, "control: stoi('abc') throws invalid_argument");
		expect(a2i(std::string("abc")) == 0, "a2i('abc') -> 0 instead of terminate");

		bool threw_range = false;
		try {
			(void)std::stoi(std::string("999999999999"));
		} catch (const std::out_of_range&) {
			threw_range = true;
		}
		expect(threw_range, "control: stoi(overflow) throws out_of_range");
		expect(a2i(std::string("999999999999")) == INT_MAX,
			"a2i(overflow) -> INT_MAX instead of terminate");
	}

	// ---- Return type contract: int32, usable where atoi was.
	static_assert(std::is_same_v<decltype(a2i("1")), int32>,
		"a2i returns int32");

	// ---- Part 5: a2l (atol migration). In-range and saturation parity
	// with the real libc atol; nullptr is the new defined case (legacy
	// atol(NULL) segfaulted — and every atol site fed it Sql_GetData()
	// output that can legitimately be nullptr on an empty result set).
	static const char* const long_parity[] = {
		"0", "42", "-42", "  7", "12abc", "", "abc", "-0",
		"2147483647", "-2147483648", "4294967296", "-4294967296",
		"9223372036854775807", "-9223372036854775807",
		"99999999999999999999999", "-99999999999999999999999",
	};
	for (const char* s : long_parity) {
		const long want = std::atol(s);
		const long got = a2l(s);
		if (got != want) {
			std::printf("FAIL: a2l parity for '%s': atol=%ld a2l=%ld\n",
				s, want, got);
			++failures;
		}
	}
	expect(a2l(nullptr) == 0, "a2l(nullptr) -> 0 instead of segfault");

	// ---- Part 6: a2f (atof migration). In-range parity with libc
	// atof; NULL / "nan" / "inf" / overflow become 0.0 — the values
	// that used to reach float->int casts as NaN/±inf (undefined).
	static const char* const float_parity[] = {
		"0", "1.5", "-2.25", "0.0", "  3.14", "-.5", "5.", "1e2",
		"12abc", "abc", "", "  ", "12.34.56", "-0.0",
	};
	for (const char* s : float_parity) {
		const double want = std::atof(s);
		const double got = a2f(s);
		if (got != want) {
			std::printf("FAIL: a2f parity for '%s': atof=%g a2f=%g\n", s, want, got);
			++failures;
		}
	}
	{
		struct nonfinite_case { const char* in; };
		static const nonfinite_case cases[] = {
			{"nan"}, {"NaN"}, {"inf"}, {"-inf"}, {"infinity"}, {"1e400"}, {"-1e400"},
		};
		for (const auto& c : cases) {
			const double libc_value = std::atof(c.in); // control: ±inf or NaN
			if (a2f(c.in) != 0.0) {
				std::printf("FAIL: a2f('%s') = %g, want 0 (libc=%g)\n",
					c.in, a2f(c.in), libc_value);
				++failures;
			}
		}
		std::printf("a2f non-finite/overflow cases: %zu all -> 0 (libc gave %g / %g)\n",
			sizeof(cases) / sizeof(cases[0]), std::atof("nan"), std::atof("1e400"));
	}
	expect(a2f(nullptr) == 0.0, "a2f(nullptr) -> 0");

	// ---- Site A/B: the @rates clamp now happens in the double domain
	// before the (int32) cast. For every input where the legacy cast
	// was defined, the result must be identical.
	for (const char* message : {"0", "0.5", "1", "100", "100.5", "50.99",
			"-3", "-0.001", "33.333", "99.999", "100.0", "101", "500"}) {
		int32 legacy_rate;
		{
			double drate = std::atof(message);
			legacy_rate = (int32)(drate * 100);
			if (legacy_rate < 0) legacy_rate = 0;
			if (legacy_rate > 10000) legacy_rate = 10000;
		}
		int32 new_rate;
		{
			double drate = a2f(message) * 100;
			if (drate < 0) new_rate = 0;
			else if (drate > 10000) new_rate = 10000;
			else new_rate = (int32)drate;
			if (new_rate < 0) new_rate = 0;
			if (new_rate > 10000) new_rate = 10000;
		}
		if (legacy_rate != new_rate) {
			std::printf("FAIL: rates A/B for '%s': legacy=%d new=%d\n",
				message, legacy_rate, new_rate);
			++failures;
		}
	}

	// ---- The pc_split_atoui chain now sees finite doubles only:
	// a2f("nan") = 0 falls into its <0/else branches definedly where
	// the raw NaN used to fall through both comparisons into a UB cast.
	{
		const double f = a2f("nan");
		uint32 value;
		if (f < 0) value = 0;
		else if (f > UINT_MAX) value = UINT_MAX;
		else value = static_cast<uint32>(f);
		expect(value == 0, "pc_split chain with former-NaN input -> 0 defined");
	}

	if (failures == 0) {
		std::printf("ALL A2I CHECKS PASSED\n");
		return 0;
	}
	std::printf("%d FAILURES\n", failures);
	return 1;
}
