# rAthena Toolchain & Dependency Modernization (C++23 baseline)

Foundation/modernization pass only. No intentional gameplay, packet, script,
database, combat, timer, networking, or user-facing behavior changes.

## Toolchain baseline

| Item | Before | After |
|---|---|---|
| C++ standard | C++17 (`gnu++17` in practice) | ISO C++23 strict (`CMAKE_CXX_EXTENSIONS OFF` / `-std=c++23`) |
| Primary compiler (Linux) | GCC 8-era | GCC 16.2 (verified 2026-08-07 release, latest stable; GCC 17 is trunk-only) |
| Secondary compiler (CI) | — | Clang 22.1.8 (latest stable MSYS2/LLVM; no nightlies) |
| CMake minimum | 3.13 | 3.28 (newest *reasonable* minimum, not newest release; CI may use latest stable) |
| CMake C++ handling | global `CMAKE_CXX_STANDARD`, `COMPILE_FLAGS` strings | strict default + `rathena_cxx23` (`cxx_std_23`, extensions off) and `rathena_warnings` interface targets |
| MSVC | C++17 (`stdcpp17`) | C++23 (`stdcpp23`, VS 2022 17.8+, `/std:clatest`-capable) |
| Autotools | `-std=c++17` | `-std=c++23` (retained and updated; `configure` reproduced from `configure.ac` with GNU Autoconf 2.69 — the only non-vanilla block is the distro-backported `--runstatedir`, already present before this pass) |
| Release flags | `-O3` via CMake defaults, `-ffast-math` in Autotools CFLAGS | conservative `-O2`; `-ffast-math` removed; LTO opt-in (`-DENABLE_LTO`); no `-march=native` |
| Sanitizers | none | opt-in `-DENABLE_SANITIZERS=address,undefined[,thread]` (never in Release) |
| Arch handling | 64-bit pointer == x64 | explicit x86_64/AMD64 vs AArch64/ARM64 detection (`architecture` = `x86`/`x64`/`arm64`) |

C++23 is the latest finalized, published ISO standard (C++26 still draft).
Compiler minimums (GCC 14+, Clang 18+, MSVC 19.38+) reflect complete enough
C++23 support for the subset used, not mere `-std=c++23` recognition.

## Source compatibility fixes for C++23 (behavior-preserving)

- `src/common/winapi.cpp`: MSVC-only SEH (`__try`/`__finally`/`__leave`) →
  plain block + `goto cleanup`. Same cleanup order, no `__except` existed.
- Strict mode drops the implicit `WIN32` predefine (only defined under
  `gnu++NN`): `-DWIN32` now comes from CMake on `WIN32` builds.
- `src/common/winapi.hpp`: `NOMINMAX` guarded (MinGW predefines it);
  `NONLS` removed (it excludes `winnls.h`, breaking cpp-httplib).
- `src/tool/*`: `tools` interface target now propagates global definitions;
  bogus file-path include entry removed.

## PCRE2 behavior notes

- `preg_match` and chat patterns keep the historic vector capacity: the old
  30-int PCRE1 ovector yielded 10 capture pairs (its last third was
  workspace). PCRE2 match data is created with 10 pairs, so the
  `r == 0 → push 30/3` quirk for patterns with more captures than fit is
  preserved exactly (pinned as cases 43-45; PCRE2 10.49 also returns 0
  there).
- `src/map/npc_chat.cpp`: for an *unset optional group* the old code copied
  into an uninitialized stack buffer (nondeterministic). The PCRE2 path sets
  an empty string instead — a deliberate, documented normalization, not
  pinned behavior.
- Failed compiles and negative match results keep the old observable timing
  (no match / push 0); PCRE2 error codes and offsets are engine diagnostics
  and are not surfaced.
- `PCRE2_CODE_UNIT_WIDTH=8` is set by the vendored CMake target (Windows),
  the MSVC vcxproj files, the `pcre2_system` interface target (system
  PCRE2), and `PCRE_CFLAGS` (autotools); `pcre2.h` has no fallback width.

## Dependency audit

Pinned to exact stable tags; no downloads at build time; offline builds keep
working from a complete checkout.

| Dependency | Before | After | Class | Notes |
|---|---|---|---|---|
| cpp-httplib | 0.11.1 | **0.60.0** | small compat migration | multipart split into `fields`/`files`; `http.hpp` helpers preserve old behavior. License MIT |
| nlohmann/json | 3.11.2 | **3.12.0** | safe direct update | single-header swap. License MIT |
| yaml-cpp | ~0.8.0 (commit `0e6e28d`) | **0.9.0** (pending) | safe direct update | verify file list vs rAthena build glue |
| rapidyaml (+c4core) | 0.4.0 / c4core 0.1.8 | **0.16.0** | small compat migration | `>>` kept via documented `RYML_WITH_LEGACY_OPERATORS` (`.deserialize()` migration is follow-up); Callbacks split 1→3 handlers re-registered to keep throwing `std::runtime_error`; `Parser::parse_in_arena` removed → free function; read-only API to `ConstNodeRef`; c4core submodule gone, required sources vendored. License MIT |
| libconfig | 1.4.9 | **1.8.2** (pending) | safe direct update | C library; check scanner/grammar regeneration needs. License LGPL-2.1+ |
| zlib | 1.2.8 headers + prebuilt Win DLL | **1.3.2** (not 1.3.1 — 1.3.2 is the actual latest stable, Feb 2026) | safe direct update | prebuilt blobs removed; static build from vendored source on Windows (new zlib.vcxproj, `.sln` wired, consumers link `.vs\build\zlib.lib`); system zlib elsewhere. No more runtime zlib.dll. License Zlib |
| PCRE (8.33, PCRE1) | 8.33 | **10.49** (PCRE2) | substantial migration | 8-bit PCRE2 built from vendored source on Windows (prebuilt pcre8 DLL/import-lib blobs removed, no runtime DLL) and system `libpcre2-8` elsewhere. API/behavior differences isolated and pinned by `tools/pcre-regress` (48 cases byte-for-byte vs the PCRE1 8.33 baseline); JIT sources compiled but never invoked. License BSD-3-Clause |
| MySQL client | 6.1.9 headers / 5.7.16 | **deferred** | architectural | Windows-only bundled compat headers; system client on Linux. Replacing needs DBA-level validation |

## Validation (local: MinGW-w64 ucrt64, CMake 4.4.4, Ninja)

- [x] GCC 16.2 + C++17 baseline green (before switch)
- [x] Clang 22.1.8 + C++17 baseline green
- [x] GCC 16.2 + strict C++23 green (all servers + tools)
- [x] Clang 22.1.8 + strict C++23 green
- [x] Warning set wired to all owned targets (no `-Werror` yet).
  Baseline GCC C++23: ~41k diagnostics, dominated by `-Wswitch` (~17k),
  `-Wsign-conversion` (~6k), `-Wpedantic`/`-Wunused-parameter` (~5-6k each),
  `-Wconversion` (~4k). Mostly repeated-template-header noise + game-code
  enum/parameter style. Burn-down + `-Werror` is follow-up work, not a mass
  rewrite inside this pass.
- [x] YAML smoke (rapidyaml 0.16): const/item_equip/item_etc/mob/skill/
  achievement DBs parse — 6+12982+9959+2675+1635+361 entries, zero errors
- [x] PCRE2 parity: `tools/pcre-regress` (10.49) reproduces the pinned
  PCRE 8.33 output byte-for-byte over 48 chat/preg_match cases, including
  the 30-int-ovector == 10-pair capture capacity boundary and the
  match-limit / catastrophic-backtracking error path; CI runs the harness
  under ASan/LSan (`linux-pcre-regress`) against the committed baseline.
- [x] Autotools: `configure` regenerated from `configure.ac` with GNU
  Autoconf 2.69 (the generator version of the committed file) matches
  byte-for-byte except the distro-backported `--runstatedir` block; the
  hand-written "kept in sync manually" note and the stale PCRE heading were
  removed, so no hand edits remain.
- [x] GCC 16.2 + PCRE2 (vendored static): map-server rebuild green.
- [x] Clang 22.1.8 + PCRE2: migrated TUs (`src/map/script.cpp`,
  `src/map/npc_chat.cpp`) parse clean (`-fsyntax-only`), warnings unchanged.
- [x] System-PCRE2 CMake path exercised (`-DWITH_LOCAL_PCRE=OFF`) with a
  simulated system package: consumers receive `-DPCRE2_CODE_UNIT_WIDTH=8`
  from the new `pcre2_system` interface target.
- [x] Clean configure without workarounds (in-tree ws2_32/getaddrinfo/librt
  fixes verified: `build-clean` configures with zero extra flags)
- [~] Sanitizers: `-DENABLE_SANITIZERS=address,undefined` plumbing configures
  and every TU *compiles* with instrumentation, but MinGW-w64 (GCC and Clang)
  ships no ASan/UBSan runtimes (`cannot find -lasan/-lubsan`; even UBSan-trap
  mode needs libubsan at link). Runtime jobs are Linux-CI-only by necessity.
- [ ] Windows MSVC C++23 build (vcxproj updated, needs VS 2022 17.8+ host)
- [ ] AArch64 compile coverage (CI)
- [ ] Sanitizer build + startup smoke (login/char/map start, YAML parse, scripts load)
- [ ] Windows MSVC C++23 build (vcxproj updated, needs VS 2022 17.8+ host)
- [ ] AArch64 compile coverage (CI)

## CI matrix (intended)

Linux: GCC-latest+C++23 (Release, Debug, ASan+UBSan), Clang-latest+C++23
(Release), AArch64 compile check. Windows: MSVC stable + C++23.
`-Werror` in modern-toolchain jobs once the warning baseline is clean.

## CI matrix (implemented: `.github/workflows/modern-toolchain.yml`)

- `linux-gcc` (Release + Debug, GCC 14): configure + full build + map-server
  boot smoke (expected to stop at absent-DB connect, exit 1 — any other
  failure, e.g. crash, fails the job).
- `linux-clang` (Release, Clang 18): configure + full build.
- `linux-pcre-regress`: builds `tools/pcre-regress` against system
  `libpcre2-8` with ASan/LSan and diffs its output against the committed
  PCRE 8.33 baseline (48 cases). No full build, no PCRE1 DLL required.
- `linux-sanitizers` (Debug, ASan+UBSan via `-DENABLE_SANITIZERS`):
  build + login/map startup smoke. Full sanitizer *runtime* coverage is
  Linux-only (MinGW ships no ASan/UBSan runtimes); all TUs were verified to
  *compile* under instrumentation locally.
- `linux-arm64` (native `ubuntu-24.04-arm`): full build. No arch-specific
  game logic; the top-level `architecture` var distinguishes `x64` vs `arm64`.
- `windows-msvc` (windows-2022): CMake configure + Release build
  (C++23 via `stdcpp23` vcxproj; needs host verification on VS 2022 17.8+).

All Linux jobs install `libpcre2-dev` and assert `Enabled PCRE code` during
configure, so a silently PCRE-less build fails fast (it would otherwise skip
`PCRE_SUPPORT` code entirely).
- Full login+char+map boot *with* MariaDB stays in
  `tools/docker/docker-compose.yml` (unchanged flow; `./configure` there now
  defaults to `-std=c++23`).
