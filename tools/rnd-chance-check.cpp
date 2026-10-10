// rnd-chance-check: boundary + probabilistic contract test for rnd_chance
// (src/common/random.hpp), covering the battle.cpp probability gates.
//
// Convention (same as tools/pcre-regress): standalone, NOT wired into any
// build on purpose. Run by hand when touching RNG usage:
//   g++ -std=c++23 -Wall -Wextra -Isrc tools/rnd-chance-check.cpp -o /tmp/rnd-chance-check && /tmp/rnd-chance-check
// Exit 0 on success; nonzero + FAILED lines on failure.
#include <cstdint>
#include <cstdio>

#include "common/random.hpp"

static int failures = 0;

#define EXPECT_TRUE(x)                                                                                  \
	do {                                                                                            \
		if (!(x)) {                                                                             \
			std::printf("FAILED line %d: expected true\n", __LINE__);                       \
			++failures;                                                                     \
		}                                                                                       \
	} while (0)

#define EXPECT_FALSE(x)                                                                                 \
	do {                                                                                            \
		if (x) {                                                                                \
			std::printf("FAILED line %d: expected false\n", __LINE__);                      \
			++failures;                                                                     \
		}                                                                                       \
	} while (0)

int main()
{
	// Deterministic boundaries: 0 can never succeed, >= 100 always does.
	for (int i = 0; i < 1000; ++i) {
		EXPECT_FALSE(rnd_chance(0, 100));
		EXPECT_TRUE(rnd_chance(100, 100));
		EXPECT_TRUE(rnd_chance(150, 100));
		EXPECT_FALSE(rnd_chance(-5, 100));
		EXPECT_FALSE(rnd_chance<uint16_t>(0, 100));
		EXPECT_TRUE(rnd_chance<uint16_t>(100, 100));
	}
	// Probabilistic contract: 50/100 over many draws stays near one half.
	// Tolerance +-0.02 at N=200000 is ~18 sigma: failure means a broken
	// generator, not a flake.
	const int N = 200000;
	int hits = 0;
	for (int i = 0; i < N; ++i)
		hits += rnd_chance(50, 100) ? 1 : 0;
	const double rate = (double)hits / (double)N;
	std::printf("observed rate: %.4f (N=%d)\n", rate, N);
	if (rate < 0.48 || rate > 0.52) {
		std::printf("FAILED: rate outside [0.48, 0.52]\n");
		++failures;
	}
	if (failures == 0)
		std::printf("ALL CHECKS PASSED\n");
	return failures;
}
