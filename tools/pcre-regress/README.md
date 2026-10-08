# PCRE regression harness (dev-only)

Pins the regular-expression behavior rAthena exposes (`npc chat` patterns and
`preg_match`) against a frozen PCRE 8.33 (PCRE1) baseline, so the engine swap to
PCRE2 is validated byte-for-byte instead of "looks fine at runtime".

Not wired into any build on purpose: run it by hand when touching PCRE usage or
updating the vendored engine.

## Files

- `corpus.txt` — one case per line: `pattern \t subject \t offset \t caseless(0/1)`.
  `#` starts a comment line. Cases 43-45 pin the capture-vector capacity
  boundary (the old 30-int PCRE1 ovector yielded exactly 10 capture pairs);
  the final case pins the match-limit / catastrophic-backtracking error path.
- `pcre-regress.cpp` — the harness (PCRE2 8-bit; `PCRE2_CODE_UNIT_WIDTH=8` is
  defined in the source so it also builds standalone).
- `pcre1.expected` — frozen output of the PCRE 8.33 run. This is the source of
  truth; the PCRE2 port must reproduce it exactly.
- `check-diag.sh` — asserts the `--diag` error categories (see below).

## Check PCRE2 against the baseline

Build against the vendored 8-bit library (after a normal CMake build):

```sh
g++ -std=c++23 -DPCRE_SUPPORT -DPCRE2_STATIC -I 3rdparty/pcre2/include \
    tools/pcre-regress/pcre-regress.cpp lib/libpcre2_localr.a -o pcre-regress.exe
./pcre-regress.exe tools/pcre-regress/corpus.txt > /tmp/pcre2.out
diff <(tr -d '\r' < tools/pcre-regress/pcre1.expected) <(tr -d '\r' < /tmp/pcre2.out)
```

A non-empty diff rejects the change. (Windows console output is CRLF; both
sides get normalized. Against a system PCRE2, link `-lpcre2-8` instead of the
archive.)

## Error categories (`--diag`)

`rAthena` only ever observes "matched / did not match", so `pcre1.expected`
normalizes every negative `pcre2_match()` status away. The distinction still
matters when diagnosing a pattern, so `--diag` prints the engine status per
case instead:

```sh
./pcre-regress --diag tools/pcre-regress/corpus.txt
# 1|match|-1|NOMATCH     26|match|-33|BADOFFSET    45|match|0|OVEC_SMALL
# 27|compile_err|114     33|match|1|OK            47|match|-47|LIMIT
```

Categories: `OK` (r > 0), `OVEC_SMALL` (r == 0: matched, more than 10 capture
pairs), `NOMATCH`, `LIMIT` (`MATCHLIMIT`/`DEPTHLIMIT`/`HEAPLIMIT`),
`BADOFFSET`, `NOMEMORY`, `ERROR`; compile failures print
`<index>|compile_err|<code>`. This output is never diffed — assert it with
`check-diag.sh <file>`, which pins one case per category and exits non-zero on
any mismatch.

## CI

`linux-pcre-regress` (`.github/workflows/modern-toolchain.yml`) compiles this
harness under ASan/LSan **twice** — against the system `libpcre2-8` and against
the vendored 10.49 sources — and diffs each run against `pcre1.expected`, so
parity and resource cleanup are checked for both PCRE2 variants on every run
without needing the old PCRE1 DLL. Each run then goes through
`check-diag.sh` to pin the error categories.

## Regenerating the baseline (PCRE1 only, when the corpus grows)

Use the PCRE 8.33 header and x64 DLL that the pre-PCRE2 commit keeps in history
(`3rdparty/pcre/`); MinGW links the DLL directly. Never regenerate
`pcre1.expected` from PCRE2 — that would defeat the pin.
