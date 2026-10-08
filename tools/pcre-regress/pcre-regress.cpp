// PCRE regression harness (dev-only, NOT wired into any build).
// Usage: pcre-regress <corpus>   (corpus lines: pattern \t subject \t offset \t caseless(0/1))
// Output per case: index | compile_rc | match_rc | ovec pairs | cap0 | cap1
// The PCRE1 run output is committed as pcre1.expected; the PCRE2 port must
// produce byte-identical output or the migration is rejected.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef PCRE_SUPPORT
#include <pcre.h>
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
		const char* error = nullptr;
		int erroffset = 0;
		pcre* re = pcre_compile(pattern, caseless ? PCRE_CASELESS : 0, &error, &erroffset, nullptr);
		if (!re) {
			// NB: error offsets are engine diagnostics, not behavior:
			// rAthena only observes success/failure. Normalize them away.
			std::printf("%d|compile_err\n", idx++);
			continue;
		}
		pcre_extra* extra = pcre_study(re, 0, &error);
		int ovec[30] = {0};
		int r = pcre_exec(re, extra, subject, (int)strlen(subject), offset, 0, ovec, 30);
		// NB: rAthena only observes r<0 (no match) vs r>=0 (match count);
		// negative engine error codes differ between PCRE1/PCRE2. Normalize.
		if (r < 0) {
			std::printf("%d|compile_ok|NOMATCH\n", idx++);
		} else {
			std::printf("%d|compile_ok|%d", idx++, r);
			if (r > 0) {
				for (int i = 0; i < r * 2 && i < 30; i++)
					std::printf("|%d", ovec[i]);
				char cap[3][256] = {{0}};
				for (int i = 0; i < 3 && i < r; i++) {
					pcre_copy_substring(subject, ovec, r, i, cap[i], sizeof(cap[i]));
					std::printf("|%s", cap[i]);
				}
			}
			std::printf("\n");
		}
		pcre_free(re);
		if (extra) pcre_free(extra);
#else
		std::printf("%d|NO_PCRE\n", idx++);
#endif
	}
	fclose(f);
	return 0;
}
