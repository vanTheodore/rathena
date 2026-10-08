// PCRE regression harness (dev-only, NOT wired into any build).
// Usage: pcre-regress <corpus>          pinned parity output (diffed against pcre1.expected)
//        pcre-regress --diag <corpus>   engine status categories, one line per case
//                                        (index|match|<rc>|<category> or index|compile_err|<code>).
//                                        Never diffed: it asserts that "no match", execution
//                                        limits and real errors stay distinct inside the engine
//                                        although the two rAthena call sites collapse them.
//                                        See doc/MODERNIZATION.md and check-diag.sh.
// Corpus lines: pattern \t subject \t offset \t caseless(0/1)
// Output per case: index | compile_ok | match_rc | ovec pairs | cap0 | cap1
// The PCRE1 8.33 run output is committed as pcre1.expected; the PCRE2 port must
// produce byte-identical output or the migration is rejected.
//
// Build against the vendored 8-bit PCRE2 (CMake target pcre2_local).
// Example (MinGW ucrt64, after a regular CMake build):
//   g++ -std=c++23 -DPCRE_SUPPORT -DPCRE2_STATIC -I 3rdparty/pcre2/include tools/pcre-regress/pcre-regress.cpp lib/libpcre2_localr.a -o pcre-regress.exe
// Against a system libpcre2-8, link -lpcre2-8 instead of the archive.
//
// Capacity note: PCRE1 pcre_exec() with the 30-int ovector used by rAthena
// could return at most 10 capture pairs (the last third of the vector was
// workspace). Corpus cases 43-45 pin that boundary, so this harness creates
// its match data with exactly 10 pairs; rAthena's call sites do the same.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef PCRE_SUPPORT
#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#endif
#include <pcre2.h>
#endif

// PCRE1 pcre_exec() ovecsize=30 ints => 10 usable capture pairs.
static const int OVEC_PAIRS = 10;

#ifdef PCRE_SUPPORT
// Categories for --diag. rAthena's call sites deliberately collapse every
// negative status into "no match" (PCRE1 behaved the same), so the pinned
// output cannot show them; --diag exposes the engine-level distinction the
// runtime intentionally discards.
static const char* status_category(int r) {
	if (r > 0) return "OK";          // real match with r capture pairs
	if (r == 0) return "OVEC_SMALL"; // matched, but the 10-pair ovector was too small
	switch (r) {
	case PCRE2_ERROR_NOMATCH: return "NOMATCH";
	case PCRE2_ERROR_MATCHLIMIT:
	case PCRE2_ERROR_DEPTHLIMIT:
	case PCRE2_ERROR_HEAPLIMIT: return "LIMIT"; // execution/resource limits
	case PCRE2_ERROR_BADOFFSET: return "BADOFFSET";
	case PCRE2_ERROR_NOMEMORY: return "NOMEMORY";
	default: return "ERROR";
	}
}
#endif

static std::vector<std::string> split_tab(const std::string& s) {
	std::vector<std::string> out;
	size_t b = 0;
	for (;;) {
		size_t t = s.find('\t', b);
		if (t == std::string::npos) { out.push_back(s.substr(b)); break; }
		out.push_back(s.substr(b, t - b));
		b = t + 1;
	}
	return out;
}

int main(int argc, char** argv) {
	bool diag = false;
	const char* corpus = nullptr;
	if (argc == 2) {
		corpus = argv[1];
	} else if (argc == 3 && std::strcmp(argv[1], "--diag") == 0) {
		diag = true;
		corpus = argv[2];
	} else {
		std::printf("usage: pcre-regress [--diag] <corpus>\n");
		return 2;
	}
#ifndef PCRE_SUPPORT
	(void)diag; // no engine in this build: both modes emit the same marker
#endif
	FILE* f = fopen(corpus, "r");
	if (!f) { std::printf("cannot open %s\n", corpus); return 2; }
	char line[8192];
	int idx = 0;
	while (fgets(line, sizeof(line), f)) {
		std::string s = line;
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		if (s.empty() || s[0] == '#') continue;
		auto cols = split_tab(s);
		if (cols.size() < 4) { std::printf("%d|BADLINE\n", idx++); continue; }
		const char* pattern = cols[0].c_str();
		const char* subject = cols[1].c_str();
		int offset = atoi(cols[2].c_str());
		int caseless = atoi(cols[3].c_str());
#ifdef PCRE_SUPPORT
		int errorcode = 0;
		PCRE2_SIZE erroffset = 0;
		pcre2_code* re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
			caseless ? PCRE2_CASELESS : 0, &errorcode, &erroffset, nullptr);
		if (!re) {
			// NB: error codes/offsets are engine diagnostics, not behavior:
			// rAthena only observes success/failure. The pinned output
			// normalizes them away; --diag keeps the code so the category can
			// be asserted.
			if (diag)
				std::printf("%d|compile_err|%d\n", idx++, errorcode);
			else
				std::printf("%d|compile_err\n", idx++);
			continue;
		}
		pcre2_match_data* md = pcre2_match_data_create(OVEC_PAIRS, nullptr);
		int r = pcre2_match(re, (PCRE2_SPTR)subject, (PCRE2_SIZE)std::strlen(subject),
			(PCRE2_SIZE)offset, 0, md, nullptr);
		// NB: rAthena only observes r<0 (no match) vs r>=0 (match count);
		// negative engine error codes differ between PCRE1/PCRE2. Normalize.
		if (diag) {
			std::printf("%d|match|%d|%s\n", idx++, r, status_category(r));
		} else if (r < 0) {
			std::printf("%d|compile_ok|NOMATCH\n", idx++);
		} else {
			std::printf("%d|compile_ok|%d", idx++, r);
			if (r > 0) {
				PCRE2_SIZE* ovec = pcre2_get_ovector_pointer(md);
				for (int i = 0; i < r * 2 && i < OVEC_PAIRS * 2; i++)
					std::printf("|%d", (int)ovec[i]);
				char cap[3][256] = {{0}};
				for (int i = 0; i < 3 && i < r; i++) {
					PCRE2_SIZE caplen = sizeof(cap[i]);
					cap[i][0] = '\0';
					// Unset optional groups: same empty capture the PCRE1
					// harness printed (its buffer stayed zero-initialized).
					if (pcre2_substring_copy_bynumber(md, i, (PCRE2_UCHAR*)cap[i], &caplen) < 0)
						cap[i][0] = '\0';
					std::printf("|%s", cap[i]);
				}
			}
			std::printf("\n");
		}
		pcre2_match_data_free(md);
		pcre2_code_free(re);
#else
		std::printf("%d|NO_PCRE\n", idx++);
#endif
	}
	fclose(f);
	return 0;
}
