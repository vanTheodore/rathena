// PCRE regression harness (dev-only, NOT wired into any build).
// Usage: pcre-regress <corpus>   (corpus lines: pattern \t subject \t offset \t caseless(0/1))
// Output per case: index | compile_rc | match_rc | ovec pairs | cap0 | cap1
// The PCRE1 8.33 run output is committed as pcre1.expected; the PCRE2 port must
// produce byte-identical output or the migration is rejected.
//
// Build against the vendored 8-bit PCRE2 (CMake target pcre2_local).
// Example (MinGW ucrt64, after a regular CMake build):
//   g++ -std=c++23 -DPCRE_SUPPORT -DPCRE2_STATIC -I 3rdparty/pcre2/include \
//       tools/pcre-regress/pcre-regress.cpp lib/libpcre2_localr.a -o pcre-regress.exe
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
	if (argc != 2) { std::printf("usage: pcre-regress <corpus>\n"); return 2; }
	FILE* f = fopen(argv[1], "r");
	if (!f) { std::printf("cannot open %s\n", argv[1]); return 2; }
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
			// rAthena only observes success/failure. Normalize them away.
			std::printf("%d|compile_err\n", idx++);
			continue;
		}
		pcre2_match_data* md = pcre2_match_data_create(OVEC_PAIRS, nullptr);
		int r = pcre2_match(re, (PCRE2_SPTR)subject, (PCRE2_SIZE)std::strlen(subject),
			(PCRE2_SIZE)offset, 0, md, nullptr);
		// NB: rAthena only observes r<0 (no match) vs r>=0 (match count);
		// negative engine error codes differ between PCRE1/PCRE2. Normalize.
		if (r < 0) {
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
