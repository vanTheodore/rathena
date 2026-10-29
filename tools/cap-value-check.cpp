// cap-value-check: equivalence + evaluation-contract test for cap_value
// (src/common/utils.hpp), migrated from a triple-evaluating macro to a
// constexpr function template with the macro's ternary as its body.
//
// Legacy macro replica kept here as a test-only negative control (never
// production). It re-evaluates its value argument up to three times and
// each bound up to twice — for in-range arguments that is three full
// evaluations of whatever the caller wrote (13 call sites pass atoi(...),
// others pass apply_rate(...), pc_maxparameter(...), ...). The template
// evaluates every argument exactly once and must otherwise be value- and
// type-identical, because its body IS the macro expansion verbatim.
//
// Convention (same as tools/rnd-chance-check): standalone, NOT wired into
// any build on purpose. Run by hand when touching cap_value:
//   g++ -std=c++23 -Wall -Wextra -Wconversion -Wsign-conversion -Isrc tools/cap-value-check.cpp -o /tmp/cap-value-check && /tmp/cap-value-check
// Exit 0 + "ALL CAPPED" on success; nonzero + FAILED lines on failure.
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "common/utils.hpp"

// --- negative control: the old macro (test-only, never production) ---
#define cap_value_legacy(a, min, max) (((a) >= (max)) ? (max) : ((a) <= (min)) ? (min) : (a))
// --- end of negative control ---

static int failures = 0;

#define EXPECT_TRUE(x)                                                                                  \
	do {                                                                                           \
		if (!(x)) {                                                                           \
			std::printf("FAILED line %d: expected true\n", __LINE__);                     \
			++failures;                                                                  \
		}                                                                                      \
	} while (0)

#define EXPECT_FALSE(x)                                                                                 \
	do {                                                                                           \
		if (x) {                                                                             \
			std::printf("FAILED line %d: expected false\n", __LINE__);                    \
			++failures;                                                                  \
		}                                                                                      \
	} while (0)

template <typename T, typename LO, typename HI>
static void check_equiv(const char *name, T value, LO lo, HI hi)
{
	const auto legacy = cap_value_legacy(value, lo, hi);
	const auto modern = cap_value(value, lo, hi);
	const bool same = (legacy == modern) &&
		// value- and type-level agreement: identical result type after
		// cv/ref stripping (the macro could yield an lvalue when every
		// operand was a same-typed lvalue; no call site uses that —
		// audited — so stripped comparison is the contract).
		std::is_same_v<std::remove_cvref_t<decltype(legacy)>,
			std::remove_cvref_t<decltype(modern)>>;
	std::printf("%-24s legacy=%.17g modern=%.17g %s\n", name, static_cast<double>(legacy),
		static_cast<double>(modern), same ? "same" : "DIFFER");
	if (!same) {
		std::printf("FAILED %s: legacy and modern disagree\n", name);
		++failures;
	}
}

int main()
{
	// --- boundary matrix on int32, literal bounds (the dominant shape) ---
	{
		const int lo = -10, hi = 10;
		check_equiv<int>("int below", -11, lo, hi);
		check_equiv<int>("int at-lo", -10, lo, hi);
		check_equiv<int>("int mid", 0, lo, hi);
		check_equiv<int>("int at-hi", 10, lo, hi);
		check_equiv<int>("int above", 11, lo, hi);
	}
	// --- unsigned value vs signed literal bounds (0, USHRT_MAX / INT_MIN,
	//     INT_MAX patterns, 56+18+9 call sites) ---
	{
		check_equiv<uint32_t>("uint32 vs (0,USHRT)", 65535u, 0, 65535);
		check_equiv<uint32_t>("uint32 over", 70000u, 0, 65535);
		check_equiv<uint32_t>("uint32 under", 0u, 0, 65535);
		check_equiv<uint32_t>("uint32 vs (1,UINTMAX)", 4294967295u, 1, 4294967295u);
		const int32_t damage = 100, original = 40;
		check_equiv<int32_t>("damage diff vs INT", damage - original, INT32_MIN, INT32_MAX);
	}
	// --- negative bounds (-MAX_REFINE pattern) and negative values ---
	{
		const int refine = -3, grade = -1;
		check_equiv<int>("neg value neg bounds", refine, -10, 10);
		check_equiv<int>("neg grade", grade, -5, 5);
		check_equiv<int>("min==max degenerate", 7, 7, 7);
		check_equiv<int>("min>max inverts to hi", 5, 10, 0); // same rule in both
	}
	// --- wider/narrower + floating + computed bounds/expressions ---
	{
		check_equiv<int64_t>("int64 vs INT64-ish", (int64_t)1 << 40, 0, (int64_t)1 << 41);
		check_equiv<double>("double mid", 3.5, 0.0, 10.0);
		check_equiv<double>("double above", 13.5, 0.0, 10.0);
		const int a = 7, b = 9;
		check_equiv<int>("expression arg", a + b, a, b + 5); // (16 vs 14 => hi)
		check_equiv<uint32_t>("computed bounds", 5u, 1u + 1u, 10u - 1u);
		check_equiv<int16_t>("int16 vs SHRT", 32000, -32768, 32767);
	}
	// --- enum-ish integer constants (SZ_SMALL/SZ_BIG, BOUND_* patterns) ---
	{
		check_equiv<int>("enum-style bounds", 2, 0, 3);
	}

	// --- negative control: the legacy macro evaluates the value argument
	//     up to three times in-range; the template exactly once. ---
	{
		int calls = 0;
		auto counted = [&calls]() { ++calls; return 42; };
		const int legacy_result = cap_value_legacy(counted(), 0, 100);
		const int legacy_calls = calls;
		calls = 0;
		const int modern_result = cap_value(counted(), 0, 100);
		const int modern_calls = calls;
		std::printf("control-evals: legacy=%d call(s) modern=%d call(s)\n", legacy_calls, modern_calls);
		EXPECT_TRUE(legacy_result == 42 && modern_result == 42);
		EXPECT_TRUE(legacy_calls == 3); // pins WHY the macro had to go
		EXPECT_TRUE(modern_calls == 1);
	}

	// --- constexpr-ness (impossible with the macro) ---
	static_assert(cap_value(500, 0, 100) == 100, "constexpr clamp at hi");
	static_assert(cap_value(-5, 0, 100) == 0, "constexpr clamp at lo");
	static_assert(cap_value(50, 0, 100) == 50, "constexpr pass-through");
	static_assert(cap_value(-3, -10, 10) == -3, "constexpr negative domain");
	static_assert(std::is_same_v<decltype(cap_value(std::declval<uint32_t>(), 0, 100)),
		std::remove_cvref_t<decltype(cap_value_legacy(std::declval<uint32_t>(), 0, 100))>>,
		"result type identical for the (uint32, int, int) shape");
	static_assert(std::is_same_v<decltype(cap_value(std::declval<int>(), 0, 100)),
		std::remove_cvref_t<decltype(cap_value_legacy(std::declval<int>(), 0, 100))>>,
		"result type identical for the (int, int, int) shape");

	if (failures == 0)
		std::printf("ALL CAPPED\n");
	return failures;
}
