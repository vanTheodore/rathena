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
| Autotools | `-std=c++17` | `-std=c++23` (retained and updated; `configure` hand-synced, no autoconf on host) |
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
| zlib | 1.2.8 headers | **1.3.1** (pending) | safe direct update | CVE-2016-9840 et al fixed since 1.2.8; 1.3.1 is the stable line. License Zlib |
| PCRE (8.33, PCRE1) | 8.33 | **deferred** | substantial migration | PCRE1 EOL; PCRE2 changes regex API/behavior → isolated follow-up with compat validation, not smuggled in here |
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
- [ ] Clean configure without local MinGW workarounds (in-tree ws2_32/librt fixes)
- [ ] Sanitizer build + startup smoke (login/char/map start, YAML parse, scripts load)
- [ ] Windows MSVC C++23 build (vcxproj updated, needs VS 2022 17.8+ host)
- [ ] AArch64 compile coverage (CI)

## CI matrix (intended)

Linux: GCC-latest+C++23 (Release, Debug, ASan+UBSan), Clang-latest+C++23
(Release), AArch64 compile check. Windows: MSVC stable + C++23.
`-Werror` in modern-toolchain jobs once the warning baseline is clean.
