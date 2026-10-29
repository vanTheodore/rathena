// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Equivalence/contract test for the arr_move template and the ARR_MOVE
// macro shim in src/common/db.hpp (pcre-regress convention: standalone):
//
//   zig/clang: c++ -std=c++23 -Wall -Wextra -Wconversion -Wsign-conversion
//              -Isrc tools/arr-move-check.cpp -o arr-move-check (then run it)
//
// Legacy = verbatim replica of the historic three-memmove macro. For
// trivially-copyable elements both engines must produce byte-identical
// arrays; the modern path additionally supports non-trivial types and
// constexpr, which the memmove sequence could not.

#include <common/db.hpp>

#include <cstdio>
#include <cstring> // memmove for the legacy replica below (GCC does not
                   // pull it in transitively; production call sites did)
#include <string>

// Verbatim replica of the historic macro.
#define ARR_MOVE_LEGACY(__from, __to, __arr, __type) \
	do{ \
		if( (__from) != (__to) ) \
		{ \
			__type __backup__; \
			memmove(&__backup__, (__arr)+(__from), sizeof(__type)); \
			if( (__from) < (__to) ) \
				memmove((__arr)+(__from), (__arr)+(__from)+1, ((__to)-(__from))*sizeof(__type)); \
			else if( (__from) > (__to) ) \
				memmove((__arr)+(__to)+1, (__arr)+(__to), ((__from)-(__to))*sizeof(__type)); \
			memmove((__arr)+(__to), &__backup__, sizeof(__type)); \
		} \
	}while(0)

static int failures = 0;

static void expect(bool ok, const char* what) {
	if (!ok) {
		std::printf("FAIL: %s\n", what);
		++failures;
	}
}

// Runs both engines over a copy of `in` and compares every element.
template <typename T, std::size_t N>
static void check_equiv(const char* name, const T (&in)[N], ptrdiff_t from, ptrdiff_t to) {
	T a[N], b[N];
	for (std::size_t k = 0; k < N; ++k) {
		a[k] = in[k];
		b[k] = in[k];
	}
	ARR_MOVE_LEGACY(from, to, a, T);
	ARR_MOVE(from, to, b, T);
	for (std::size_t k = 0; k < N; ++k) {
		if (!(a[k] == b[k])) {
			std::printf("FAIL: %s at [%zu] legacy=%d modern=%d (from=%td to=%td)\n",
				name, k, static_cast<int>(a[k]), static_cast<int>(b[k]), from, to);
			++failures;
			return;
		}
	}
}

int main() {
	const int base[] = {10, 20, 30, 40, 50, 60};
	const int one[] = {7};
	const int two[] = {1, 2};

	// ---- from < to: adjacent and distant shifts
	check_equiv<int>("f<to adjacent", base, 1, 2);
	check_equiv<int>("f<to distant", base, 0, 5);
	check_equiv<int>("f<to end", base, 2, 5);
	// ---- from > to: left shifts
	check_equiv<int>("f>to adjacent", base, 3, 2);
	check_equiv<int>("f>to distant", base, 5, 0);
	check_equiv<int>("f>to start", base, 4, 0);
	// ---- identity and degenerate containers
	check_equiv<int>("same index", base, 3, 3);
	check_equiv<int>("single elem", one, 0, 0);
	check_equiv<int>("two fwd", two, 0, 1);
	check_equiv<int>("two back", two, 1, 0);

	// ---- Element-position contract: element originally at `from`
	// lands at `to`, everything else shifts by one (checked explicitly,
	// not just legacy-vs-modern).
	{
		int v[] = {10, 20, 30, 40, 50, 60};
		ARR_MOVE(1, 4, v, int);
		const int want[] = {10, 30, 40, 50, 20, 60};
		for (int k = 0; k < 6; ++k)
			expect(v[k] == want[k], "explicit layout from<to");
		int w[] = {10, 20, 30, 40, 50, 60};
		ARR_MOVE(4, 1, w, int);
		const int want2[] = {10, 50, 20, 30, 40, 60};
		for (int k = 0; k < 6; ++k)
			expect(w[k] == want2[k], "explicit layout from>to");
	}

	// ---- The two production call shapes (char_mapif.cpp fame_list):
	// struct-typed entries via an explicit __type argument.
	{
		struct fame_list { int value; char name[24]; };
		fame_list f[] = {{1, "a"}, {2, "b"}, {3, "c"}};
		ARR_MOVE(2, 0, f, struct fame_list);
		expect(f[0].value == 3 && f[1].value == 1 && f[2].value == 2,
			"struct fame_list rotation");
	}

	// ---- Non-trivial type: well-defined via move-assignment (the legacy
	// memmove sequence would alias owning strings). Modern-only pin.
	{
		std::string s[] = {"a", "b", "c"};
		ARR_MOVE(2, 0, s, std::string);
		expect(s[0] == "c" && s[1] == "a" && s[2] == "b", "std::string rotation");
	}

	// ---- constexpr capability (impossible with memmove)
	{
		struct run { static constexpr int go() {
			int v[] = {1, 2, 3, 4};
			ARR_MOVE(0, 3, v, int);
			return (v[0] == 2 && v[1] == 3 && v[2] == 4 && v[3] == 1) ? 0 : 1;
		} };
		static_assert(run::go() == 0, "ARR_MOVE usable at compile time");
	}

	if (failures == 0) {
		std::printf("ALL MOVE CHECKS PASSED\n");
		return 0;
	}
	std::printf("%d FAILURES\n", failures);
	return 1;
}
