// apply-rate-check: equivalence + single-evaluation regression test for the
// apply_rate/apply_rate2 function templates (src/common/utils.hpp).
//
// Convention (same as tools/pcre-regress): standalone, NOT wired into any
// build on purpose. Run by hand when touching rate math:
//   g++ -std=c++23 -Wall -Wextra -Wconversion -Wsign-conversion -Isrc tools/apply-rate-check.cpp -o /tmp/apply-rate-check && /tmp/apply-rate-check
// Exit 0 + "ALL EQUIVALENT" on success; nonzero + MISMATCH lines on failure.
// (No -Werror: the legacy control below intentionally triggers
// -Wsign-conversion at its expansion sites — that is part of what the test
// documents. The templates must produce zero warnings; verify in the log.)
//
// The _legacy macros below are the pre-template definitions, kept ONLY as
// the negative control: the single-evaluation assertion FAILS for them
// (they evaluate the value argument 2-3 times) and PASSES for the templates.
// They must never return to production code.
#include <cstdint>
#include <cstdio>

#include "common/utils.hpp"

// --- negative control: the old macro definitions (test-only) ---
// Kept ONLY so the single-evaluation assertion below can prove it detects
// the dangerous behavior (legacy evaluates 2-3x, template 1x). These macros
// intentionally produce -Wsign-conversion at expansion sites; the templates
// must not. They must never return to production code.
#define apply_rate_legacy(val, rate) (((rate) == 100) ? (val) : ((val) > 100000) ? ((val) / 100 * (rate)) : ((val) * (rate) / 100))
#define apply_rate2_legacy(val, rate, per) (((rate) == (per)) ? (val) : ((val) > 100000) ? ((val) / (per) * (rate)) : ((val) * (rate) / (per)))
// --- end of negative control ---

static int failures = 0;

#define CHECK_EQ(a, b)                                                                                  \
	do {                                                                                            \
		auto _a = (a);                                                                          \
		auto _b = (b);                                                                          \
		if (_a != _b) {                                                                         \
			std::printf("MISMATCH line %d: %lld vs %lld\n", __LINE__, (long long)_a,         \
				(long long)_b);                                                         \
			++failures;                                                                         \
		}                                                                                       \
	} while (0)

// The templates are constexpr: pin the fast path and a boundary at compile time.
static_assert(apply_rate(200, 100) == 200);
static_assert(apply_rate(100000, 50) == 50000);
static_assert(apply_rate(100001, 50) == 50000); // (100001/100)*50, integer division
static_assert(apply_rate2(200, 50, 100) == 100);

int main()
{
	// Value grid: 0/1, around the 100000 overflow-avoidance boundary, large.
	const long long vals[] = { 0, 1, 99, 100, 101, 99999, 100000, 100001, 1000000, 2000000000LL };
	const long long rates[] = { 0, 1, 50, 99, 100, 101, 150, 200 };
	for (long long v : vals)
		for (long long r : rates) {
			CHECK_EQ(apply_rate_legacy(v, r), apply_rate(v, r));
			CHECK_EQ(apply_rate2_legacy(v, r, 100), apply_rate2(v, r, 100));
			CHECK_EQ(apply_rate2_legacy(v, r, 1), apply_rate2(v, r, 1));
		}
	// Mixed-type shapes exactly as used at the 24 call sites.
	CHECK_EQ(apply_rate_legacy(50000u, 150), apply_rate(50000u, 150));
	CHECK_EQ(apply_rate_legacy(50000, -5), apply_rate(50000, -5));
	CHECK_EQ(apply_rate_legacy(200000u, -5), apply_rate(200000u, -5));
	CHECK_EQ(apply_rate_legacy((short)30000, 150), apply_rate((short)30000, 150));
	CHECK_EQ(apply_rate_legacy(5000000000ULL, 150), apply_rate(5000000000ULL, 150));
	CHECK_EQ(apply_rate_legacy(0u, 0), apply_rate(0u, 0));
	CHECK_EQ(apply_rate_legacy((uint16_t)1000, (int32_t)150), apply_rate((uint16_t)1000, (int32_t)150)); // rate_fix
	CHECK_EQ(apply_rate_legacy((int64_t)500, (int32_t)150), apply_rate((int64_t)500, (int32_t)150)); // delay
	CHECK_EQ(apply_rate_legacy((uint32_t)100000, (int8_t)100), apply_rate((uint32_t)100000, (int8_t)100)); // hp/int8
	CHECK_EQ(apply_rate_legacy((uint32_t)100000, -(int8_t)100), apply_rate((uint32_t)100000, -(int8_t)100)); // -hp_rate
	CHECK_EQ(apply_rate_legacy((int32_t)500, (int32_t)150), apply_rate((int32_t)500, (int32_t)150));
	CHECK_EQ(apply_rate_legacy((uint32_t)500, (int32_t)150), apply_rate((uint32_t)500, (int32_t)150));
	CHECK_EQ(apply_rate2_legacy(50000u, 150, 100), apply_rate2(50000u, 150, 100));
	CHECK_EQ(apply_rate2_legacy(200000u, -5, 100), apply_rate2(200000u, -5, 100));
	{
		double d1 = apply_rate2_legacy(1000, 50, 1), d2 = apply_rate2(1000, 50, 1);
		if (d1 != d2) {
			std::printf("MISMATCH double\n");
			++failures;
		}
	}
	{
		double d1 = apply_rate_legacy(100.0, (int32_t)150), d2 = apply_rate(100.0, (int32_t)150);
		if (d1 != d2) {
			std::printf("MISMATCH double/int32\n");
			++failures;
		}
	}
	// Single-evaluation control: a value argument with a side effect must be
	// evaluated exactly once. The legacy macro evaluates it twice here (it
	// would fail this assertion); the template must pass it.
	{
		int legacy_calls = 0, tmpl_calls = 0;
		auto legacy_arg = [&](int x) { ++legacy_calls; return x; };
		auto tmpl_arg = [&](int x) { ++tmpl_calls; return x; };
		(void)apply_rate_legacy(legacy_arg(200000), 50);
		(void)apply_rate(tmpl_arg(200000), 50);
		std::printf("legacy evals of val arg: %d, template evals: %d\n", legacy_calls, tmpl_calls);
		if (legacy_calls < 2) {
			std::printf("CONTROL VOID: legacy macro evaluated once, detection unproven\n");
			++failures;
		}
		if (tmpl_calls != 1) {
			std::printf("TEMPLATE EVALUATED MORE THAN ONCE\n");
			++failures;
		}
	}
	if (failures == 0)
		std::printf("ALL EQUIVALENT\n");
	return failures;
}
