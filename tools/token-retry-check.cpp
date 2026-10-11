// token-retry-check: contract regression test for mmo_auth_token_retry
// (src/login/token_retry.hpp), the bounded retry behind web_auth_token
// refreshes. Scripted outcomes drive the real helper: no database, fully
// deterministic, no hangs possible (every case is attempt-bounded).
//
// Convention (same as tools/apply-rate-check): standalone, NOT wired into
// any build on purpose. Build and run by hand when touching the helper:
//   g++ -std=c++23 -Wall -Wextra -Isrc -IC:/msys64/ucrt64/include/mariadb tools/token-retry-check.cpp -o C:/rathena-test/token-retry-check.exe && C:/rathena-test/token-retry-check.exe
// Exit 0 + "ALL CONTRACT" on success; nonzero + FAIL lines on failure.

#include <cstdio>
#include <vector>

#include <login/token_retry.hpp>

// Scripted query executor: replays canned (result, errno) pairs.
struct Script {
	std::vector<int> results; // 0 = SQL_SUCCESS, nonzero = SQL_ERROR
	std::vector<unsigned> errnos;
	size_t at = 0;
	int calls = 0;
	unsigned last_errno = 0;

	int run() {
		if( at >= results.size() )
			return 0; // scripts must be long enough; fail loudly below instead
		last_errno = errnos[at];
		int r = results[at];
		++at;
		++calls;
		return r;
	}
	unsigned err() const { return last_errno; }
};

static int failures = 0;

#define EXPECT_CASE(name, script, want_ok, want_attempts) do { \
		int32 attempts = -1; \
		Script s script; \
		bool ok = mmo_auth_token_retry([&]() -> int32 { return s.run(); }, \
			[&]() -> uint32 { return s.err(); }, 20, &attempts); \
		bool pass = (ok == want_ok) && (attempts == want_attempts); \
		if( !pass ) ++failures; \
		std::printf("%s %s (ok=%d attempts=%d)\n", pass ? "PASS" : "FAIL", name, (int)ok, (int)attempts); \
	} while(0)

int main() {
	// 1. First-try success: single attempt, immediate return.
	EXPECT_CASE("success-first", ({ {0}, {0} }), true, 1);
	// 2. Transient collisions then success: recovers on attempt 3.
	EXPECT_CASE("transient-then-success", ({ {1,1,0}, {1062,1062,0} }), true, 3);
	// 3. Persistent collision: stops at exactly MAX_RETRIES (off-by-one guard).
	{
		int32 attempts = -1;
		Script s{ std::vector<int>(30, 1), std::vector<unsigned>(30, 1062) };
		bool ok = mmo_auth_token_retry([&]() -> int32 { return s.run(); },
			[&]() -> uint32 { return s.err(); }, 20, &attempts);
		bool pass = (!ok) && (attempts == 20) && (s.calls == 20);
		if( !pass ) ++failures;
		std::printf("%s persistent-1062 (ok=%d attempts=%d calls=%d)\n",
			pass ? "PASS" : "FAIL", (int)ok, (int)attempts, s.calls);
	}
	// 4. Non-duplicate error: single attempt, no retry.
	EXPECT_CASE("non-duplicate", ({ {1}, {1054} }), false, 1);
	// 5. Collision then non-duplicate: stops at the second attempt.
	EXPECT_CASE("collision-then-other", ({ {1,1}, {1062,2006} }), false, 2);
	// 6. Success landing exactly on the last allowed attempt.
	{
		int32 attempts = -1;
		std::vector<int> r(20, 1); std::vector<unsigned> e(20, 1062);
		r[19] = 0; e[19] = 0;
		Script s{ r, e };
		bool ok = mmo_auth_token_retry([&]() -> int32 { return s.run(); },
			[&]() -> uint32 { return s.err(); }, 20, &attempts);
		bool pass = (ok) && (attempts == 20);
		if( !pass ) ++failures;
		std::printf("%s success-on-20th (ok=%d attempts=%d)\n",
			pass ? "PASS" : "FAIL", (int)ok, (int)attempts);
	}
	// 7. Success waiting past the limit must NOT run a 21st attempt.
	{
		int32 attempts = -1;
		std::vector<int> r(21, 1); std::vector<unsigned> e(21, 1062);
		r[20] = 0; e[20] = 0;
		Script s{ r, e };
		bool ok = mmo_auth_token_retry([&]() -> int32 { return s.run(); },
			[&]() -> uint32 { return s.err(); }, 20, &attempts);
		bool pass = (!ok) && (attempts == 20) && (s.calls == 20);
		if( !pass ) ++failures;
		std::printf("%s no-21st-attempt (ok=%d attempts=%d calls=%d)\n",
			pass ? "PASS" : "FAIL", (int)ok, (int)attempts, s.calls);
	}

	if( failures == 0 )
		std::printf("ALL CONTRACT\n");
	return failures;
}
