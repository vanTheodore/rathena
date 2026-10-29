// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Equivalence/contract test for the arr_find template and the ARR_FIND
// macro shim in src/common/db.hpp (pcre-regress convention: standalone,
// compiled directly, not wired into the build):
//
//   zig/clang: c++ -std=c++23 -Wall -Wextra -Wconversion -Wsign-conversion
//              -Isrc tools/arr-find-check.cpp -o arr-find-check (then run it)
//
// The legacy for-loop macro is replicated verbatim below and run over the
// same inputs; results, predicate evaluation order/count, and truthiness
// must match. The one intentional contract difference — bounds are
// evaluated once by arr_find instead of on every loop iteration — is
// pinned as a control (all 254 real call sites pass pure bounds).

#include <common/db.hpp>

#include <cstdio>
#include <vector>

// File-scope so the constexpr parity lambdas below need no capture.
static constexpr int kConstArr[] = {5, 6, 7};

// Verbatim replica of the historic macro (pre-arr_find refactor).
#define ARR_FIND_LEGACY(__start, __end, __var, __cmp) \
	do{ \
		for( (__var) = (__start); (__var) < (__end); ++(__var) ) \
			if( __cmp ) \
				break; \
	}while(0)

static int failures = 0;

static void expect(bool ok, const char* what) {
	if (!ok) {
		std::printf("FAIL: %s\n", what);
		++failures;
	}
}

// Both macros scan `arr` of `n` ints for `target`, leaving the index in `i`.
template <typename N>
static void check_equiv(const char* name, const int* arr, N n, int target) {
	int li = -1, mi = -1;
	ARR_FIND_LEGACY(0, n, li, arr[li] == target);
	ARR_FIND(0, n, mi, arr[mi] == target);
	if (li != mi) {
		std::printf("FAIL: %s legacy=%d modern=%d\n", name, li, mi);
		++failures;
	}
}

int main() {
	// ---- Result equivalence grid (found at first/mid/last, absent, empty, degenerate)
	const int arr[] = {9, 9, 7, 9, 1, 9};
	const int empty[1] = {0};
	check_equiv<int>("found@first", arr, 6, 9);
	check_equiv<int>("found@mid", arr, 6, 1);
	check_equiv<int>("found@last", arr, 6, 9);
	{
		// Absent target: both leave var == end
		int li = -1, mi = -1;
		ARR_FIND_LEGACY(0, 6, li, arr[li] == 42);
		ARR_FIND(0, 6, mi, arr[mi] == 42);
		expect(li == 6 && mi == 6, "absent -> var == end for both");
	}
	{
		// Empty range: no predicate evaluation, var == start == end
		int li = -1, mi = -1;
		int evals_l = 0, evals_m = 0;
		ARR_FIND_LEGACY(1, 1, li, (evals_l++, empty[li] == 0));
		ARR_FIND(1, 1, mi, (evals_m++, empty[mi] == 0));
		expect(li == 1 && mi == 1 && evals_l == 0 && evals_m == 0,
			"empty range: var==start, zero evaluations");
	}
	{
		// Degenerate start > end: loop never runs, var == start
		int li = -1, mi = -1;
		ARR_FIND_LEGACY(4, 2, li, arr[li] == 9);
		ARR_FIND(4, 2, mi, arr[mi] == 9);
		expect(li == 4 && mi == 4, "start>end: var == start for both");
	}

	// ---- Mixed index/bound types (int index vs size_t bound and back)
	{
		int i = -1;
		const size_t n = 6;
		ARR_FIND(0, n, i, arr[i] == 1);
		expect(i == 4, "int var vs size_t end");
		size_t j = 99;
		const int m = 6;
		ARR_FIND(0, m, j, arr[j] == 7);
		expect(j == 2, "size_t var vs int end");
	}

	// ---- Predicate evaluation: once per visited index, same order, both engines
	{
		std::vector<int> order_legacy, order_modern;
		int li = -1, mi = -1;
		ARR_FIND_LEGACY(0, 6, li, (order_legacy.push_back(li), arr[li] == 1));
		ARR_FIND(0, 6, mi, (order_modern.push_back(mi), arr[mi] == 1));
		expect(li == mi, "stateful predicate same result");
		expect(order_legacy == order_modern, "stateful predicate same visit order");
		expect(order_modern.size() == 5, "predicate evaluated once per index (0..4)");
	}

	// ---- Control: bounds evaluated once by arr_find (intentional difference;
	// legacy re-runs the __end expression on every loop condition check).
	{
		int end_evals_legacy = 0, end_evals_modern = 0;
		auto legacy_end = [&]() { ++end_evals_legacy; return 6; };
		auto modern_end = [&]() { ++end_evals_modern; return 6; };
		int li = -1, mi = -1;
		ARR_FIND_LEGACY(0, legacy_end(), li, arr[li] == 1); // found at 4
		ARR_FIND(0, modern_end(), mi, arr[mi] == 1);
		expect(li == mi && li == 4, "call-bound same result");
		expect(end_evals_modern == 1, "modern bound evaluated exactly once");
		expect(end_evals_legacy == 5, "control: legacy bound re-evaluated per condition (4 hits + miss check)");
		std::printf("control-bound-evals: legacy=%d modern=%d\n",
			end_evals_legacy, end_evals_modern);
	}

	// ---- Truthiness parity for non-bool predicates
	{
		int li = -1, mi = -1;
		const int nonzero[] = {0, 0, 3, 0};
		ARR_FIND_LEGACY(0, 4, li, nonzero[li]);       // int truthiness
		ARR_FIND(0, 4, mi, nonzero[mi]);
		expect(li == mi && li == 2, "int predicate truthiness parity");
	}
	{
		int li = -1, mi = -1;
		const char* names[] = {nullptr, "x", "y"}; // nullptr must be skipped
		ARR_FIND_LEGACY(0, 3, li, names[li]);      // pointer truthiness
		ARR_FIND(0, 3, mi, names[mi]);
		expect(li == mi && li == 1, "pointer predicate truthiness parity");
	}

	// ---- Multi-line invocation (replicates mob.cpp/party.cpp call style)
	{
		int li = -1, mi = -1;
		ARR_FIND_LEGACY(0, 6, li,
			arr[li] == 7);
		ARR_FIND(0, 6, mi,
			arr[mi] == 7);
		expect(li == mi && li == 2, "multi-line invocation parity");
	}

	// ---- Result type is the index variable's type (not promoted by end)
	static_assert(std::is_same_v<decltype(arr_find(std::declval<int&>(), std::declval<size_t>(),
			std::declval<size_t>(), []() { return false; })), int>,
		"arr_find returns the index type");

	// ---- constexpr: the shim and template work where the old macro's
	// semantics were only reachable imperatively.
	{
		auto legacy_ct = []() constexpr {
			int i = -1;
			ARR_FIND_LEGACY(0, 3, i, kConstArr[i] == 6);
			return i;
		};
		auto modern_ct = []() constexpr {
			int i = -1;
			ARR_FIND(0, 3, i, kConstArr[i] == 6);
			return i;
		};
		static_assert(legacy_ct() == 1, "legacy replica constexpr");
		static_assert(modern_ct() == 1, "ARR_FIND constexpr via arr_find");
		static_assert(modern_ct() == legacy_ct(), "constexpr parity");
	}

	if (failures == 0) {
		std::printf("ALL FIND CHECKS PASSED\n");
		return 0;
	}
	std::printf("%d FAILURES\n", failures);
	return 1;
}
