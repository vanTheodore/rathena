// script-sort-check: ordering regression test for the script array index
// sort (src/map/script.cpp). The old qsort comparator subtracted unsigned
// values into int32, inverting the order of values 2^31 apart.
//
// Convention (same as tools/pcre-regress): standalone, NOT wired into any
// build on purpose. Run by hand when touching array sorting:
//   g++ -std=c++23 -Wall -Wextra -Isrc tools/script-sort-check.cpp -o /tmp/script-sort-check && /tmp/script-sort-check
// Exit 0 + "ALL SORTED" on success; nonzero + FAILED lines on failure.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// --- negative control: the old comparator (test-only, never production) ---
static int32_t index_cmp_legacy(const void *a, const void *b)
{
	return ((int32_t)(*(const uint32_t *)a - *(const uint32_t *)b));
}
// --- end of negative control ---

static int failures = 0;

static bool is_sorted_asc(const uint32_t *v, size_t n)
{
	for (size_t i = 1; i < n; ++i)
		if (v[i - 1] > v[i])
			return false;
	return true;
}

static void check_case(const char *name, const uint32_t *input, size_t n)
{
	uint32_t legacy[16], modern[16];
	if (n > 16) {
		std::printf("FAILED %s: fixture too large\n", name);
		++failures;
		return;
	}
	std::memcpy(legacy, input, n * sizeof(uint32_t));
	std::memcpy(modern, input, n * sizeof(uint32_t));
	if (n > 0) {
		std::qsort(legacy, n, sizeof(uint32_t), index_cmp_legacy);
		std::sort(modern, modern + n);
	}
	const bool legacy_ok = is_sorted_asc(legacy, n);
	const bool modern_ok = is_sorted_asc(modern, n);
	const bool same = (n == 0) || (std::memcmp(legacy, modern, n * sizeof(uint32_t)) == 0);
	std::printf("%-12s legacy=%s modern=%s same=%s\n", name, legacy_ok ? "ok" : "WRONG",
		modern_ok ? "ok" : "WRONG", same ? "yes" : "no");
	if (!modern_ok) {
		std::printf("FAILED %s: std::sort output not ascending\n", name);
		++failures;
	}
}

int main()
{
	const uint32_t normal[] = { 9, 3, 7, 1, 5 };
	const uint32_t dupes[] = { 4, 2, 4, 1, 2, 4 };
	const uint32_t extremes[] = { 0xFFFFFFFFu, 0, 1, 2147483647u, 2147483648u, 42 };
	const uint32_t single[] = { 7 };
	check_case("normal", normal, 5);
	check_case("duplicates", dupes, 6);
	check_case("single", single, 1);
	check_case("empty", nullptr, 0);
	check_case("extremes", extremes, 6);
	// In the domain where the legacy comparator was correct (values within
	// int32 range of each other), both must agree exactly.
	uint32_t small_old[5] = { 9, 3, 7, 1, 5 }, small_new[5] = { 9, 3, 7, 1, 5 };
	std::qsort(small_old, 5, sizeof(uint32_t), index_cmp_legacy);
	std::sort(small_new, small_new + 5);
	if (std::memcmp(small_old, small_new, sizeof(small_old)) != 0) {
		std::printf("FAILED small-domain: legacy and modern disagree\n");
		++failures;
	}
	// Negative control: on the extremes the legacy comparator must produce
	// a wrong order while std::sort stays ascending (proves the test can
	// catch the bug class it guards against).
	uint32_t neg[6];
	std::memcpy(neg, extremes, sizeof(neg));
	std::qsort(neg, 6, sizeof(uint32_t), index_cmp_legacy);
	if (is_sorted_asc(neg, 6)) {
		std::printf("CONTROL VOID: legacy comparator sorted the extremes, detection unproven\n");
		++failures;
	} else {
		std::printf("control: legacy misorders extremes as expected\n");
	}
	if (failures == 0)
		std::printf("ALL SORTED\n");
	return failures;
}
