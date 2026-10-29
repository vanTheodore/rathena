# libconfig vendor update — 1.8.2 → 1.8.3 (2026-10-29)

Source tag: [hyperrealm/libconfig v1.8.3](https://github.com/hyperrealm/libconfig/releases/tag/v1.8.3). libconfig is LGPL-2.1+ and the existing LICENSE is preserved.

The vendor sources (including regenerated `scanner.c`) were checked against
the official v1.8.2 sources first. Every C source/header under the local vendor
directory matched upstream v1.8.2 byte-for-byte **except**
`wincompat.h`, which contains rAthena-specific Win32 linker and MinGW
`posix_fsync` compatibility fixes. These patches are intentionally preserved.

The following files were copied from v1.8.3 byte-for-byte, with **identical
Git blob SHA values** (upstream/source checked programmatically):

| Path inside 3rdparty/libconfig | Upstream v1.8.3 blob |
| --- | --- |
| libconfig.c | 737f163e63705e7cf389852fe54a53f75792087c |
| libconfig.h | 117b775535f87d4b0414c9cdab31db04e1bf2f84 |
| parsectx.h | 113443db9d1786bd08035223536beb95b0cc3fca |
| scanctx.c | 12e7d1a5db73e5fe1d85cdd358cf4ce71a73c345 |
| scanctx.h | f7b6913f6b7e45b6ea940cc651b58e477d4c9d15 |
| scanner.c | 6cdcf2879a9b71f1039438d5773abf8ae090b27f |
| scanner.h | 20fcd99e97e8f6280592be5546cc8020f4537dd4 |
| strbuf.c | 4b5363fc0c22451a093735e793e67255e281a758 |
| strbuf.h | 5e9510e8e4aba7da12c170166bcb90bce456a379 |
| strvec.c | 02f31e0502bd49ef3cc5e7ae314fcb4e23c7feaf |
| strvec.h | 9d43f9dc63f5272f7ddd769db16a4e0a494c6a1e |
| util.c | 7d6dd22f90a44afe11284384270ad49ea67b0306 |
| util.h | 0104218cf3c077f295e1c6ceba6a3b3814de6eb8 |
| wincompat.c | abfdbcf7b26330f85d917201366eebde6c7a0ff5 |

Upstream `grammar.c` and `grammar.h` are **identical** between v1.8.2
and v1.8.3, so the original blobs remain. The patched `wincompat.h`
is updated only for its upstream copyright year, keeping the 15 lines
of rAthena local changes. The custom CMake, Makefile and MSVC build glue
are unchanged.

## Verification

- `libconfig-vendor` CI compiles the **actual** C parser/scanner with the
  bundled sources and runs a valid/invalid configuration lookup smoke.
- `modern-toolchain` CI covers Linux GCC/Clang, MSVC, ARM64 and sanitizers.
- The parser smoke is not complete parity testing for numeric edge cases or
  the entire rAthena configuration ecosystem. More exhaustive fixtures
  should follow in a distinct PR after the build gate passes.
