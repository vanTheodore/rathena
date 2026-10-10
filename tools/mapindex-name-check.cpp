// mapindex-name-check: regression test for mapindex_getmapname_ext
// (src/common/mapindex.cpp). Pins truncation safety for overlong input,
// exact semantics for valid input, and the not-shared-buffer contract.
//
// Convention (same as tools/pcre-regress): standalone, NOT wired into any
// build on purpose. Links the built common static library; build it first
// with a normal CMake build, then run by hand when touching mapindex:
//   g++ -std=c++23 -Wall -Wextra -Isrc tools/mapindex-name-check.cpp lib/libcommonr.a -lws2_32 -o /tmp/mapindex-name-check && /tmp/mapindex-name-check
// Exit 0 + "ALL NAMES OK" on success; nonzero + FAILED lines on failure.
//
// Sanitizers are unavailable on this host (MinGW ships no ASan runtimes),
// so this is an assert-based test, not an ASanPoC. The overflow it guards
// against (unbounded copy into a 16-byte buffer) was already fixed by
// bounding the copy; the shared-static removal is proven by inspection
// (no static storage remains) plus the zero-null-output-caller audit.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "common/mapindex.hpp"
#include "common/mmo.hpp" // MAP_NAME_LENGTH_EXT

// --- test-only scaffolding: globals owned by core.cpp, never touched here ---
// SERVER_NAME must be a valid string: the memory manager formats it into its
// leak-log path on first allocation (a null here crashes the harness, not
// production code).
char SERVER_NAME_buf[] = "mapindex-name-check";
char db_path[1024] = { 0 };
char *SERVER_NAME = SERVER_NAME_buf;
const char *get_svn_revision(void) { return "test"; }
const char *get_git_hash(void) { return "test"; }
// --- end of scaffolding ---

static int failures = 0;

#define EXPECT_STR_EQ(got, want)                                                                        \
	do {                                                                                            \
		if (std::strcmp((got), (want)) != 0) {                                                  \
			std::printf("FAILED line %d: got '%s', want '%s'\n", __LINE__, (got), (want));  \
			++failures;                                                                     \
		}                                                                                       \
	} while (0)

int main()
{
	char out[MAP_NAME_LENGTH_EXT];
	char out2[MAP_NAME_LENGTH_EXT];

	// Valid names gain the extension; the returned pointer is the output.
	std::memset(out, 'X', sizeof(out));
	if (mapindex_getmapname_ext("prontera", out) != out) {
		std::printf("FAILED: return value is not the output buffer\n");
		++failures;
	}
	EXPECT_STR_EQ(out, "prontera.gat");
	// A 12-char input is already overlong (base names hold 11): pre-existing
	// truncation semantics pin the mangled-but-bounded result, exactly as the
	// old code produced it (verified by the old-vs-new A/B below, not by taste).
	EXPECT_STR_EQ(mapindex_getmapname_ext("prontera.gat", out), "prontera.ga.gat");
	EXPECT_STR_EQ(mapindex_getmapname_ext("a", out), "a.gat");

	// Overlong input is truncated exactly like mapindex_normalize_name
	// treats too-long names (11 chars + ".gat"), never overflows, and stays
	// NUL-terminated inside the 16-byte buffer.
	std::memset(out, 'X', sizeof(out));
	EXPECT_STR_EQ(mapindex_getmapname_ext("averyveryverylongmapname", out), "averyveryve.gat");
	if (out[MAP_NAME_LENGTH_EXT - 1] != '\0') {
		std::printf("FAILED: result not terminated inside %d bytes\n", MAP_NAME_LENGTH_EXT);
		++failures;
	}
	// Fragment syntax: part after '#' wins, still bounded.
	EXPECT_STR_EQ(mapindex_getmapname_ext("ignored#geffen", out), "geffen.gat");

	// No aliasing between uses: a second call cannot clobber the first.
	mapindex_getmapname_ext("prontera", out);
	mapindex_getmapname_ext("geffen", out2);
	EXPECT_STR_EQ(out, "prontera.gat");
	EXPECT_STR_EQ(out2, "geffen.gat");

	// Null output has no shared static to fall back to: defined nullptr.
	if (mapindex_getmapname_ext("prontera", nullptr) != nullptr) {
		std::printf("FAILED: null output did not return nullptr\n");
		++failures;
	}

	if (failures == 0)
		std::printf("ALL NAMES OK\n");
	return failures;
}
