# Dependency audit — 2026-10-29

Source: published stable release tags from the official upstream GitHub repositories. This is an **inventory and update decision**, not a statement that every dependency was retested against the newest published version.

| Library | Version recorded in rAthena modernization | Latest stable release checked | Decision |
| --- | --- | --- | --- |
| cpp-httplib | 0.60.0 | [0.63.0](https://github.com/yhirose/cpp-httplib/releases/tag/v0.63.0) (2026-10-22) | **Update candidate**: isolate in its own PR; inspect HTTP/API changes; test authentication, routing, multipart and full web-server builds on Linux/Windows before merging |
| PCRE2 | 10.49 | [10.49](https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.49) | Current; retain pinned 48-case parity corpus |
| yaml-cpp | 0.9.0 | [0.9.0](https://github.com/jbeder/yaml-cpp/releases/tag/yaml-cpp-0.9.0) | Current |
| rapidyaml | 0.16.0 | [0.16.0](https://github.com/biojppm/rapidyaml/releases/tag/v0.16.0) | Current |
| nlohmann/json | 3.12.0 | [3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0) | Current |
| zlib | 1.3.2 | [1.3.2](https://github.com/madler/zlib/releases/tag/v1.3.2) | Current |
| libconfig | 1.8.2 | [1.8.3](https://github.com/hyperrealm/libconfig/releases/tag/v1.8.3) (2026-10-22) | **Update candidate**: review parser/numeric compatibility and fuzz fixes; rebuild Linux/Windows and run config-file regressions |
| MariaDB Connector/C (system package / reference build) | 3.4.11 | [3.4.11](https://github.com/mariadb-corporation/mariadb-connector-c/releases/tag/v3.4.11) | Current reference client; CI uses distro package rather than pinning 3.4.11 |

All version comparisons here are based on the versions recorded in `doc/MODERNIZATION-ROADMAP.md` and release metadata. Before an actual vendor update, compare the vendored file hashes/patches with their upstream tag and preserve required build glue.

## Additional upgrade gate for libconfig

The 1.8.3 release fixes checked size arithmetic for internal collection growth, scanner stream-read errors, and several nondecimal numeric conversion behaviors. Review the [official changelog](https://github.com/hyperrealm/libconfig/releases/tag/v1.8.3), retain rAthena's Windows vcxproj/CMake glue, and add at least valid/invalid config and numeric boundary cases before merging. A compiler-only pass does not establish parser parity.

## Safe upgrade gate for cpp-httplib

1. Record the current 0.60.0 vendored tree hash and all local modifications, especially `3rdparty/httplib/httplib.cc`, CMake and MSVC glue.
2. Compare 0.60.0→0.63.0 release notes/API changes, including changes to request headers and multipart/form data.
3. Replace the vendored library as one isolated commit, preserving upstream LICENSE and build-specific adaptation in separate commits.
4. Require the **full modern-toolchain matrix**, plus web endpoint integration tests and malformed-input tests; a green compilation alone is not sufficient for networking code.
5. Merge only after observed behavior is compatible. Do not silently adjust gameplay/network protocols.

## CI policy

The long-running `modern-toolchain` suite executes on pull requests and pushes to `master`, avoiding duplicate full matrices for a single feature-branch push. The independent `modernization-contracts` suite uses GCC 14 and Clang 18 and runs nine standalone C++23 tests on PRs and `master`. No global `-Werror` was added because the existing warning baseline is not yet clean.

## Known limits

- Standalone tests for storage/script sorting pin local models of the production comparator; they protect the documented ordering contract but **do not** replace integration tests against production call sites.
- `rnd-chance-check` uses a 200,000-draw statistical gate, with a documented 18-sigma tolerance, not a seed-locked PRNG snapshot.
- Database-backed functionality requires its own service/integration tests.
