// storage-sort-check: ordering contract test for storage_sortitem
// (src/map/storage.cpp), which migrated from qsort + a three-way C
// comparator to std::sort + a strict-weak-ordering predicate.
//
// The legacy comparator is kept here as a test-only replica (negative
// control, never production). It is NOT a strict weak ordering: for two
// empty slots (nameid == 0 || amount == 0) it returns > 0 in both
// directions, so its output for them was unspecified even under qsort.
// It also subtracts unsigned nameids into int32, inverting the order of
// values 2^31 apart. std::sort must not receive it verbatim — passing a
// non-SWO comparator is undefined behaviour (libstdc++ can partition
// past the end of the range), which is exactly what this test pins down.
//
// The modern predicate (replica of storage_item_less in storage.cpp):
// non-empty items ascending by nameid — identical to the legacy result on
// every pair where legacy was consistent — and empty slots after all
// non-empty ones, equivalent among themselves.
//
// Convention (same as tools/pcre-regress): standalone, NOT wired into any
// build on purpose. Run by hand when touching storage sorting:
//   g++ -std=c++23 -Wall -Wextra tools/storage-sort-check.cpp -o /tmp/storage-sort-check && /tmp/storage-sort-check
// Exit 0 + "ALL SORTED" on success; nonzero + FAILED lines on failure.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Mirror of the fields struct item (src/common/mmo.hpp) that the
// comparator touches: t_itemid nameid (uint32) and int32 amount.
struct item {
	uint32_t nameid;
	int32_t amount;
};

// --- negative control: the old comparator (test-only, never production) ---
static int storage_comp_item_legacy(const void *_i1, const void *_i2)
{
	const struct item *i1 = (const struct item *)_i1;
	const struct item *i2 = (const struct item *)_i2;

	if (i1->nameid == i2->nameid)
		return 0;
	else if (!(i1->nameid) || !(i1->amount))
		return 1;
	else if (!(i2->nameid) || !(i2->amount))
		return -1;

	return (int)(i1->nameid - i2->nameid);
}
// --- end of negative control ---

// Replica of storage_item_less (src/map/storage.cpp).
static bool storage_item_less_modern(const struct item &i1, const struct item &i2)
{
	const bool empty1 = (i1.nameid == 0 || i1.amount == 0);
	const bool empty2 = (i2.nameid == 0 || i2.amount == 0);

	if (empty1 != empty2)
		return empty2;
	if (empty1)
		return false;
	return i1.nameid < i2.nameid;
}

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

// Verifies the observable contract of the modern sort on a result buffer:
// every non-empty item (nameid != 0 && amount != 0) before every empty
// one, non-empty nameids ascending, any order among equivalent empties.
static bool modern_order_ok(const struct item *v, size_t n)
{
	bool seen_empty = false;
	uint32_t last_nameid = 0;
	bool first = true;

	for (size_t i = 0; i < n; ++i) {
		const bool empty = (v[i].nameid == 0 || v[i].amount == 0);
		if (empty) {
			seen_empty = true;
			continue;
		}
		if (seen_empty)
			return false; // non-empty item after an empty slot
		if (!first && v[i].nameid < last_nameid)
			return false; // non-empty nameids out of ascending order
		last_nameid = v[i].nameid;
		first = false;
	}
	return true;
}

static void check_modern(const char *name, const struct item *input, size_t n)
{
	struct item buf[32];

	if (n > 32) {
		std::printf("FAILED %s: fixture too large\n", name);
		++failures;
		return;
	}
	if (n > 0)
		std::memcpy(buf, input, n * sizeof(struct item)); // no memcpy(nullptr, 0)
	if (n > 1)
		std::sort(buf, buf + n, storage_item_less_modern);
	const bool ok = modern_order_ok(buf, n);
	std::printf("%-18s modern-order=%s\n", name, ok ? "ok" : "WRONG");
	if (!ok) {
		std::printf("FAILED %s: std::sort output violates the ordering contract\n", name);
		++failures;
	}
}

// Agreement on the domain where the legacy comparator was a strict weak
// ordering (all items non-empty, nameids within int32 distance): both
// sort by nameid, so the nameid sequences must match exactly.
static void check_agree_legacy(const char *name, const struct item *input, size_t n)
{
	struct item legacy[32], modern[32];

	if (n > 32) {
		std::printf("FAILED %s: fixture too large\n", name);
		++failures;
		return;
	}
	if (n > 0) {
		std::memcpy(legacy, input, n * sizeof(struct item)); // no memcpy(nullptr, 0)
		std::memcpy(modern, input, n * sizeof(struct item));
		std::qsort(legacy, n, sizeof(struct item), storage_comp_item_legacy);
	}
	if (n > 1)
		std::sort(modern, modern + n, storage_item_less_modern);

	bool keys_equal = true;
	for (size_t i = 0; i < n; ++i)
		if (legacy[i].nameid != modern[i].nameid)
			keys_equal = false;
	const bool modern_ok = modern_order_ok(modern, n);
	std::printf("%-18s legacy-vs-modern-keys=%s modern-order=%s\n", name,
		keys_equal ? "same" : "DIFFER", modern_ok ? "ok" : "WRONG");
	if (!keys_equal) {
		std::printf("FAILED %s: legacy and modern disagree on the consistent domain\n", name);
		++failures;
	}
	if (!modern_ok) {
		std::printf("FAILED %s: modern output not ordered\n", name);
		++failures;
	}
}

int main()
{
	// 1. Realistic storage rows: unique non-empty nameids, shuffled.
	{
		const struct item in[] = {
			{ 5001, 3 }, { 3, 1 }, { 71, 12 }, { 40, 1 }, { 12345, 7 }
		};
		check_agree_legacy("unique", in, 5);
		check_modern("unique", in, 5);
	}
	// 2. Duplicate nameids: amounts tie on nameid (legacy returned 0 for
	//    equal nameids too), so only the key sequence may be compared.
	{
		const struct item in[] = {
			{ 5001, 2 }, { 3, 9 }, { 5001, 5 }, { 3, 1 }, { 5001, 1 }
		};
		check_agree_legacy("duplicates", in, 5);
		check_modern("duplicates", in, 5);
	}
	// 3. Empty slots (nameid 0 / amount 0) mixed in: modern must park all
	//    of them after the non-empty rows; legacy ordering among the
	//    empties was unspecified (see the control below), so no legacy
	//    comparison is run on this fixture.
	{
		const struct item in[] = {
			{ 0, 0 }, { 5001, 3 }, { 71, 0 }, { 0, 4 }, { 3, 1 }, { 5001, 0 }
		};
		check_modern("empty-slots", in, 6);
	}
	// 4. Degenerate sizes.
	{
		const struct item one[] = { { 5001, 1 } };
		check_modern("single", one, 1);
		check_modern("empty", nullptr, 0);
		check_agree_legacy("empty-legacy", nullptr, 0);
	}
	// 5. Agreement keeps holding once empties are *present but the legacy
	//    domain is restricted*: strip empties, sort both, compare keys.
	//    (Covered by fixtures 1-2; empties are never fed to qsort.)
	// 6. Control — why the legacy comparator could not be reused verbatim:
	//    for two empty slots it is inconsistent in both directions.
	{
		const struct item a = { 0, 0 };
		const struct item b = { 5001, 0 };
		const int ab = storage_comp_item_legacy(&a, &b);
		const int ba = storage_comp_item_legacy(&b, &a);
		std::printf("control-legacy-swo: comp(a,b)=%d comp(b,a)=%d\n", ab, ba);
		EXPECT_TRUE(ab > 0);
		EXPECT_TRUE(ba > 0);
	}
	// 7. Control — legacy subtracts unsigned nameids into int32 and
	//    misorders values 2^31 apart; modern uses operator< on uint32.
	{
		struct item legacy_in[] = { { 1u, 1 }, { 0xFFFFFFFFu, 1 } };
		struct item modern_in[] = { { 1u, 1 }, { 0xFFFFFFFFu, 1 } };
		std::qsort(legacy_in, 2, sizeof(struct item), storage_comp_item_legacy);
		std::sort(modern_in, modern_in + 2, storage_item_less_modern);
		const bool legacy_sorted = legacy_in[0].nameid < legacy_in[1].nameid;
		const bool modern_sorted = modern_in[0].nameid < modern_in[1].nameid;
		std::printf("control-overflow: legacy-ascending=%s modern-ascending=%s\n",
			legacy_sorted ? "yes" : "NO", modern_sorted ? "yes" : "NO");
		EXPECT_FALSE(legacy_sorted); // proves the control detects the bug class
		EXPECT_TRUE(modern_sorted);
	}

	if (failures == 0)
		std::printf("ALL SORTED\n");
	return failures;
}
