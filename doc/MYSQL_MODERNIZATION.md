# MySQL/MariaDB integration audit and modernization plan

Status: **stages S1 and S2 landed**, no game logic touched (branch
`deps/mysql-mariadb`).
Everything below is either executed evidence from this checkout or cited
upstream/package documentation; nothing was validated against a real database
server or on Linux/MSVC (see "Executed vs not executed").

## 1. What is integrated today

### 1.1 Client library vs server version (distinct things)

| | Windows | Linux / macOS | Notes |
|---|---|---|---|
| Client library | **MySQL Connector/C 6.1.9** (bundled) | system client, detected at configure time | `3rdparty/mysql/include/mysql_version.h`: `LIBMYSQL_VERSION "6.1.9"`, `LIBMYSQL_VERSION_ID 60109` |
| Client provenance | `3rdparty/mysql` (65 headers) + prebuilt blobs | distro packages | blobs: `lib/Win32/libmysql.dll` 5.0 MB, `x64/libmysql.dll` 5.2 MB + import libs (27-29 KB each) |
| License declared by client | `mysql_version.h`: `LICENSE GPL`, `MYSQL_COMPILATION_COMMENT "MySQL Connector/C (GPL)"` | same header on Windows; distro build | project `LICENSE` is **GPLv3** |
| Server | not vendored | not vendored | `README.md:47`: "MySQL 5 or newer / MariaDB 5 or newer" |
| Client header also carries | `MYSQL_SERVER_VERSION "5.7.16"` | — | this is a *server* version string baked into the client headers; it is not the client version |

`mysql_version.h`'s `MYSQL_VERSION_ID 50716` is the value used by rAthena's
`#if MYSQL_VERSION_ID >= …` guards (`src/common/sql.cpp:14,88`). It is a
*server-version string* from the client header, not the client library version
(`LIBMYSQL_VERSION_ID`). Today this happens to work, but it is not a reliable
feature probe (finding F4).

Local evidence (this checkout, MinGW ucrt64): `build-gcc/CMakeCache.txt` shows
`HAVE_LOCAL_MYSQL=ON`, `MYSQL_LIBRARIES=…/3rdparty/mysql/lib/x64/libmysql.lib`,
`MYSQL_SYSTEM_*=NOTFOUND` — i.e. Windows builds use the bundled 6.1.9 client.

### 1.2 API surface actually used

All database access goes through **one translation unit**: `src/common/sql.cpp`
(1023 lines) + `src/common/sql.hpp` (344 lines). 25+ server sources call the `Sql_*`
helpers and `SqlStmt`, never the C API directly.

- Connection/handles: `mysql_init`, `mysql_options(MYSQL_OPT_RECONNECT)`,
  `mysql_options(MYSQL_OPT_SSL_MODE)` (`SSL_MODE_DISABLED`),
  `mysql_real_connect`, `mysql_close`, `mysql_ping`, `mysql_errno`, `mysql_error`.
- Plain queries: `mysql_real_query`, `mysql_store_result`, `mysql_num_fields`,
  `mysql_num_rows`, `mysql_affected_rows`, `mysql_insert_id`, `mysql_fetch_row`,
  `mysql_fetch_lengths`, `mysql_free_result`,
  `mysql_real_escape_string` (fallback `mysql_escape_string`).
- Prepared statements: `mysql_stmt_init/prepare/bind_param/execute/store_result/
  bind_result/fetch/fetch_column/free_result/close/insert_id/field_count/
  param_count/num_rows/error/errno`, `MYSQL_BIND`, `MYSQL_DATA_TRUNCATED`,
  `MYSQL_NO_DATA`, plus the custom column-truncation diagnostic path.
- Type mapping: `MYSQL_TYPE_TINY/SHORT/LONG/LONGLONG/FLOAT/DOUBLE/STRING/BLOB/NULL`
  (+ signed/unsigned). `SQLDT_FLOAT`/`SQLDT_DOUBLE` are declared but have **no call
  sites** (only the enum and the bind switch) — relevant because MariaDB
  Connector/C documents a prepared-statement double conversion limitation.
- Charset: `Sql_SetEncoding` → `SET NAMES <codepage>`, fed by `default_codepage`
  and per-service `login_codepage`/`ipban_codepage`/`log_codepage`
  (`conf/inter_athena.conf:24-76`).
- Transactions: plain SQL text — `START TRANSACTION`/`COMMIT`/`ROLLBACK`
  (`src/login/account.cpp`, `src/char/char_clif.cpp`, `src/char/int_mail.cpp`).
  No autocommit toggling anywhere.
- Reconnect/keepalive: `mysql_reconnect_type` (1 = N retries then shutdown,
  2 = retry forever) and `mysql_reconnect_count` (`conf/inter_athena.conf:79-85`)
  consumed by the `ra_mysql_error_handler(2003)` path, plus a keepalive timer that
  reads `SHOW VARIABLES LIKE 'wait_timeout'` and pings 30 s before it.
- Compatibility bandaid: `#define my_bool bool` for MySQL ≥ 8.0.1
  (`src/common/sql.cpp:14-18`).

### 1.3 Build integration matrix

| Path | Detection | Link | Runtime artifact |
|---|---|---|---|
| CMake (Windows) | `3rdparty/mysql/CMakeLists.txt` → local copy `3rdparty/mysql` | `libmysql.lib` (MSVC) | copies `libmysql.dll` next to binaries (5 vcxproj `Copy` targets) |
| CMake (Linux/macOS) | `3rdparty/cmake/FindMYSQL.cmake`: include `mysql.h` in `/usr/include/mysql` etc.; lib names `libmysql`, `mysqlclient`, `mysqlclient_r` | `libmysqlclient` | system |
| Autotools | `configure.ac:1234-1268`: `mysql_config --include/--libs`, `AC_CHECK_LIB([mysqlclient], [mysql_init])`, `AC_CHECK_HEADER([mysql.h])`; **hard error** if absent | `-lmysqlclient` | system |
| Visual Studio | vcxproj include/lib paths → `3rdparty/mysql` | `libmysql.lib` | `libmysql.dll` copied by 5 server vcxproj targets (char, login, map, map-server-generator, web) |
| Docker builder | `tools/docker/Dockerfile:3` installs `mariadb-dev` (Alpine) | system `libmariadb` | system |
| CI (`modern-toolchain.yml`) | installs `libmariadb-dev` only (GCC/Clang/ASan/ARM64) | — | — |

CMake is **required** to find MySQL: `src/common/CMakeLists.txt:190` aborts with
`FATAL_ERROR` if `HAVE_LOCAL_MYSQL`/`HAVE_SYSTEM_MYSQL` are both off.

## 2. Findings

### F1 (high) — modern Debian/Ubuntu packaging is invisible to both detection paths

Ubuntu noble `libmariadb-dev` (the package CI installs) provides
`/usr/include/mariadb/mysql.h`, `libmariadb.so` / `libmariadbclient.so` and
`mariadb_config`. `/usr/include/mysql/mysql.h`, `libmysqlclient.so` and
`mysql_config` live in a *separate* package, `libmariadb-dev-compat`
(verified against the published file lists, see Sources).

Consequences:
- `FindMYSQL.cmake` searches `/usr/include/mysql` and link names
  `libmysql`/`mysqlclient`/`mysqlclient_r` → finds nothing → `HAVE_MYSQL` off →
  CMake configure aborts at `src/common/CMakeLists.txt:190`.
- Autotools needs a `mysql_config` binary; with just `libmariadb-dev` the script
  is `mariadb_config` and users must pass `--with-mysql=/usr/bin/mariadb_config`;
  the subsequent `AC_CHECK_LIB([mysqlclient], …)` still fails.

The CI matrix installs only `libmariadb-dev` (`.github/workflows/modern-toolchain.yml`
lines 23/40/100/119), so the Linux jobs would fail at configure. They were never run
(no remote), which is why this is still latent. Not executed on Linux here.

### F2 (high) — the bundled Windows client is a 2014 client that cannot authenticate against current servers

MySQL Connector/C 6.1.9 predates `caching_sha2_password` (introduced in MySQL
5.7.23/8.0). MySQL 8.4 ships with `mysql_native_password` disabled by default and
MySQL 9.x removed it, so a 6.1.9 client cannot log in to a default modern server.
MariaDB Connector/C supports `caching_sha2_password` from 3.0.8 onward. The 6.1.9
client also carries a pre-OpenSSL-1.1 TLS stack. Additionally its own header
declares `LICENSE GPL` while the project is GPLv3 — the compatibility question
(MySQL FOSS exception scope, or swapping to LGPL-2.1+ MariaDB Connector/C) needs a
licensing review; this audit does not give legal advice.

### F3 (medium) — `MYSQL_OPT_RECONNECT` is deprecated upstream

MySQL 8.0.34/8.1.0 deprecated automatic reconnection; `MYSQL_OPT_RECONNECT` is
"still available and functional" but "deprecated and will be removed in a future
version", and 8.0.34+ clients warn at runtime. rAthena depends on it for the
`mysql_reconnect_type` semantics; a connector upgrade needs the reconnect behavior
re-implemented explicitly (next-query/keepalive reconnect) if the option disappears,
preserving the existing config semantics.

### F4 (low) — version guards probe the wrong macro — **fixed in S2**

`src/common/sql.cpp:14,88` gate `my_bool` and `MYSQL_OPT_SSL_MODE` on
`MYSQL_VERSION_ID` (server-version string from the client headers, 50716 in 6.1.9)
instead of client version macros (`LIBMYSQL_VERSION_ID`, MariaDB's
`MARIADB_VERSION_ID`). Works by accident today (the 6.1.9 header does define
`MYSQL_OPT_SSL_MODE`, `mysql.h:176`); will silently misprobe once the client is
replaced. **S2** defines `RA_MYSQL_CLIENT_VERSION` (`LIBMYSQL_VERSION_ID` →
`MARIADB_VERSION_ID` → `MYSQL_VERSION_ID` → 0) and uses it in both guards; the
decision matrix is verified in section 7 (S2).

### F5 (low) — deprecated connection-less escapers

`Sql_EscapeString*` fall back to `mysql_escape_string` when the handle is null
(deprecated, thread-unsafe, removed in some modern clients). It is only reachable
before a connection exists.

### F6 (info) — behavior to keep, not "modernize"

Transactions are hand-written (`START TRANSACTION`/`COMMIT`/`ROLLBACK`),
autocommit is left at the client default, and `SQL_INNODB` (`src/common/sql.hpp:27`)
stays undefined by default, which changes how `map/log.cpp:49` batches inserts.
These are behavior, not debt: preserve them.

### F7 (medium) — 10.7 MB of unpinned prebuilt blobs

`3rdparty/mysql/lib/{Win32,x64}` ships two DLLs plus import libs, copied at build
time by five vcxproj targets. No hash pinning, no version floor, 32-bit blob is dead
weight for most users, and there is no documented upgrade path for the client.

### F8 (info) — client version is invisible at runtime — **fixed in S2**

Nothing logs `mysql_get_client_info()`; diagnosing auth/TLS failures requires
guessing which client is in use. A single startup log line is behavior-neutral and
worth adding during the migration. **S2** logs it once in `Sql_Init()`
(`MySQL client library: <version>`), and the CI boot smoke asserts the line and the
MariaDB Connector/C series in it.

### F9 (medium) — documented support matrix is stale

`README.md:47` still advertises "MySQL 5 / MariaDB 5", while MySQL 8.0 reached EOL
in April 2026 (8.4 LTS is the supported line) and MariaDB 11.x is current. The
matrix (client floor, server floor, tested combinations) should be restated once a
connector is chosen.

## 3. Alternatives

| Option | Version / status | License | API/ABI vs today | Windows | Linux | Assessment |
|---|---|---|---|---|---|---|
| Keep MySQL Connector/C 6.1.9 (bundled) | 2014, unmaintained | GPL (per its header) | current | MSVC + MinGW, blobs in-repo | n/a | Reject: no modern auth/TLS, license question, unpinned blobs |
| MySQL Connector/C 8.4 LTS | current LTS (8.0 EOL 2026-04) | GPLv2 + FOSS exception (verify per release) | same C API, newer headers | MSI/ZIP, MSVC | packages (mysql) | Viable but keeps GPL exception review and does not match the distro/CI default client |
| MySQL Connector/C 9.x | innovation line | same | same | yes | yes | Not indicated: newest ≠ best here |
| **MariaDB Connector/C 3.4.x** | **3.4.11 (2026-10-06), series 3.4 GA 2025-02; 3.3 also supported** | **LGPL-2.1+** | "exactly the same API as MySQL Connector/C for MySQL 5.5" — the API rAthena uses | MSI 32/64-bit, MSVC | packaged everywhere; already our Docker builder and CI install (`libmariadb-dev`) | **Recommended**: license fits GPLv3, modern auth (`caching_sha2_password` since 3.0.8), works with MariaDB and MySQL servers, active GA series, drop-in ABI |
| Distro-only (no bundling, any platform) | — | — | — | no system client on Windows | fine | Only half a solution; Windows still needs a client |

## 4. Recommendation

Standardize on **MariaDB Connector/C 3.4.x** as the client for both the system
(Linux/macOS) and Windows paths, while keeping the build detection able to use a
system MySQL Connector/C when one is present (secondary). This deletes the 2014
blobs, resolves the license question, restores compatibility with default MySQL
8.4+/MariaDB 11 authentication, and stops fighting distro packaging that already
ships `libmariadb`. Do not chase 3.5/preview versions; 3.4 GA is the target.

### 4.1 Client floor and how to install the client

**Floor**: MariaDB Connector/C **≥ 3.0.8** (first release with client-side
`caching_sha2_password`, what MySQL 8.4+/MariaDB 11+ servers require); **3.4.x
recommended**. A MySQL Connector/C client must be **≥ 8.0**: the 6.1.x series and
the 5.x/4.x client libraries are below the floor (no `caching_sha2_password`, no
TLS 1.2). Both build systems report and warn about this instead of failing, so
existing setups keep building while the legacy bundled client is replaced:

- CMake: `3rdparty/cmake/FindMYSQL.cmake` reports the connector kind and the
  *client* version (`mariadb_version.h`'s `MARIADB_PACKAGE_VERSION`, MySQL's
  `LIBMYSQL_VERSION`, or pkg-config as a last resort); `3rdparty/mysql/CMakeLists.txt`
  prints `Using MYSQL: <lib> (<kind> <version>)` and warns when the version is
  below the floor.
- Autotools: `configure` prints `checking MySQL client library... <kind>
  <version>` and warns when the version from `mysql_config`/`mariadb_config
  --version` is below the floor.

**Installing it (build time)**:

| Platform | Command / source | Provides |
|---|---|---|
| Debian/Ubuntu | `sudo apt-get install libmariadb-dev` (CI uses this; noble: 3.3.8) | `/usr/include/mariadb/mysql.h`, `libmariadb.so`, `libmariadb.pc` |
| Fedora/RHEL | `sudo dnf install mariadb-connector-c-devel` | same layout under `/usr/include/mariadb` |
| macOS | `brew install mariadb-connector-c` | `opt/mariadb-connector-c/{include,lib}` |
| Windows | MariaDB Connector/C 3.4.x **MSI** from mariadb.com/downloads (system install) | `%PROGRAMFILES%/MariaDB/<version>/{include,lib}` + `libmariadb.dll`; the repository stops shipping the 2014 blobs in S3 |

Detected through the distribution's paths, the `libmariadb.pc`/`mysqlclient.pc`
pkg-config modules when present (hints only — pkg-config is optional and is not
required on Windows), and MariaDB/MySQL `Program Files` layouts. No fixed paths are
assumed.

**Running the servers (runtime)**:

- The client **shared library** must be present on the machine that runs
  `login-server`/`char-server`/`map-server`: `libmariadb3` (Debian/Ubuntu),
  `mariadb-connector-c`/`mariadb-libs` (Fedora/Arch) or `libmariadb.dll`
  (Windows, shipped by the same MSI). Statically linking is not part of this plan.
- No new mandatory runtime config: the existing `inter_server`/`map_server`
  connection settings keep working. `sql_connection`-style settings, codepage
  (`SET NAMES`) and the documented privileges are unchanged; the client only
  changes *how* the connection authenticates and which TLS the client offers.
- Database-side: MariaDB 10.x/11.x or MySQL 8.0/8.4+. A server whose users use
  `caching_sha2_password` (MySQL 8.4 default) needs the ≥ 3.0.8 connector; servers
  that only accept `mysql_native_password` keep working unchanged. No schema or
  data migration is involved in this work.

## 5. Behavior that must not change

| Area | Preserved contract |
|---|---|
| Login/char/map/web flows | Same queries, same data, same account/char/inventory persistence |
| Charset | `SET NAMES <codepage>` with the configured codepage; no client-side recoding |
| Prepared statements | Same type mapping, same truncation diagnostics, same `MYSQL_DATA_TRUNCATED`/`MYSQL_NO_DATA` handling |
| NULL handling | Same `is_null` propagation (`int8` out-param) |
| Reconnect | `mysql_reconnect_type` 1/2 and `mysql_reconnect_count` semantics unchanged, including the keepalive ping 30 s before `wait_timeout` |
| Transactions | Same explicit `START TRANSACTION`/`COMMIT`/`ROLLBACK` boundaries |
| Schemas | No destructive schema/data changes; existing `sql-files` and InnoDB tables stay valid |
| Client-facing config | No new mandatory runtime config keys beyond the client library choice |

## 6. Regression and validation plan

No Docker requirement: a local/server-installed MariaDB (apt/MSYS2/Windows service)
plus the existing `sql-files` import is enough.

- **T1 build/ABI**: build all servers/tools against (a) system MariaDB Connector/C,
  (b) the chosen Windows client, GCC + Clang + MSVC, strict C++23; assert the
  configured client version is the expected one.
- **T2 real-DB conformance suite** (scenario script against a real server):
  1. create/login account, create character, save position, inventory, zeny;
  2. mail with attachment (transaction path, `int_mail.cpp`);
  3. login account save with forced failure in the middle (rollback path,
     `account.cpp`);
  4. non-ASCII round trip (e.g. `é`, `日本`) with configured codepage;
  5. NULL and boundary values through both plain queries and prepared statements;
  6. emblem upload/fetch (blob path, truncation diagnostics);
  7. reconnect test: `mysql_reconnect_type=1` and `2`, server restart/outage while
     the map server is idle and while it is busy; verify keepalive pings and the
     documented shutdown-on-exhaustion behavior;
  8. `SHOW VARIABLES LIKE 'wait_timeout'` path with a server configured to a short
     timeout.
- **T3 client comparison**: run T2 with the legacy 6.1.9 client (Windows control)
  and with the new client; results on the server side must be identical
  (same rows after the same operation sequence).
- **T4 CI**: add a Linux job that installs MariaDB server from packages, imports
  `sql-files`, runs T2 headless, and keeps the existing boot smoke. Windows job
  gains a build (and, when possible, run) step with the chosen client.
- **Acceptance**: T1 green on all three compilers; T2 all steps pass twice in a
  row; T3 shows identical DB state; no change in the reconnect/keepalive semantics
  under test step 7.

## 7. Staged implementation plan (one commit per stage, branch `deps/mysql-mariadb`)

- **S1 — detection supports the modern packaging** — **landed**. `3rdparty/cmake/FindMYSQL.cmake`
  now searches MariaDB locations/names (`/usr/include/mariadb`, `libmariadb`,
  `libmariadbclient`, MariaDB homebrew/Program Files paths) while keeping the
  MySQL names as fallback, and reports the connector version from
  `mariadb_version.h` when present. `configure.ac` falls back to `mariadb_config`
  and probes `-lmariadb`/`-lmariadbclient` after `-lmysqlclient`; `configure` was
  regenerated with GNU Autoconf 2.69 (the only non-regenerated content remains
  the distro `--runstatedir` backport, unchanged). CI now asserts
  `Configuring for system MYSQL - done` and `Found MYSQL: .*mariadb` in every
  Linux configure log.

  Verified locally: A/B test of the find module against a simulated Debian
  layout (old module: `MYSQL_FOUND` off, which is exactly the CI-breaking case;
  new module: `libmariadb.so`, version `3.4.11`) and a simulated MySQL layout
  (`libmysqlclient.so`, version `8.4.0`, no regression); `bash -n configure`;
  regenerated `configure` diffs against the previous file in exactly three hunks
  (help text, `mariadb_config` fallback, `-lmariadb`/`-lmariadbclient` probes).
  Not verified on a real Linux host (see "Executed vs not executed").
- **S2 — client-version hygiene** — **landed**. `src/common/sql.cpp` defines
  `RA_MYSQL_CLIENT_VERSION` (`LIBMYSQL_VERSION_ID` → `MARIADB_VERSION_ID` →
  `MYSQL_VERSION_ID` → 0) and uses it for the `my_bool` bandaid and the
  `MYSQL_OPT_SSL_MODE` guard, and `Sql_Init()` now logs
  `MySQL client library: <mysql_get_client_info()>` once (F4/F8).
  `3rdparty/cmake/FindMYSQL.cmake` reports the client kind and the *client* version
  (`MARIADB_PACKAGE_VERSION`, `LIBMYSQL_VERSION`, pkg-config as last resort) and
  takes optional pkg-config hints; `3rdparty/mysql/CMakeLists.txt` prints
  `Using MYSQL: <lib> (<kind> <version>)` and warns below the floor;
  `configure.ac`/`configure` print the kind and warn on the same floor (§4.1).
  CI asserts `Using MYSQL: .*mariadb.* (MariaDB Connector/C `, the absence of the
  floor warning, and the `MySQL client library: 3.` boot line.

  Verified locally: guard matrix over seven client macro combinations (6.1.9,
  8.0.34, MariaDB 3.4.11 with and without `LIBMYSQL_VERSION_ID`, 5.7.16, the
  8.0.02 quirk, no macros) extracted from the edited `sql.cpp`; floor matrix for
  CMake (12/12: MariaDB 3.4.11/3.0.8/3.0.7/2.3.7, MySQL 8.0.0/8.4.0/9.1.0/5.7.16,
  empty kind, unknown version) and for the generated shell block (11/11, including
  a MariaDB `10.3.34-MariaDB` server-style version and a trailing-space version);
  find-module probes for the four header layouts plus the pkg-config hint path
  (kind/version derived from `libmariadb`/`mysqlclient`); `bash -n configure` and
  one-hunk regeneration with GNU Autoconf 2.69; local Windows reconfigure + build
  green (95/95 steps, no `FAILED`) and the boot smoke printing
  `[Info]: MySQL client library: 6.1.9`. Not verified: Linux/real `mariadb_config`
  execution and MSVC (see §9).
- **S3 — Windows client replacement (decided: system install, no vendoring)**:
  delete the 6.1.9 headers and blobs from `3rdparty/mysql` and point detection and
  the five vcxproj targets at the MSI-installed MariaDB Connector/C 3.4.x
  (include dir, import library, matching architecture, `libmariadb.dll` next to the
  built binaries); document the required version and the runtime DLL in `README.md`
  and `3rdparty/README.txt`.
- **S4 — reconnect/keepalive**: re-implement reconnect explicitly so the
  `mysql_reconnect_type`/`mysql_reconnect_count` contract survives the removal of
  `MYSQL_OPT_RECONNECT`; keep the ping timer.
- **S5 — regression CI + docs**: land T4, restate the supported server/client
  matrix, close this document.

S1 touches no game logic and is verifiable here; S3/S4 depend on the decisions
below and on real-DB validation.

## 8. Decisions (closed)

1. Client standard: **MariaDB Connector/C 3.4.x** (LGPL-2.1+), preferably 3.4.11.
   A system MySQL Connector/C is still detected as a secondary fallback, and the
   floor in §4.1 applies to both.
2. Windows distribution: **system install via the official MSI** — no prebuilt
   blobs in the repository and no source-built vendored copy. CMake must find the
   include dir, import library, architecture and runtime DLL of that installation.
3. S1 (connector-agnostic detection) landed first, then S2 (version hygiene);
   S3/S4/S5 follow on the same branch, one commit per stage.

## 9. Executed vs not executed

Executed in this checkout: full inventory and API-surface extraction
(ripgrep), local CMake cache inspection (which client the Windows build uses),
`LICENSE`/`README`/conf/CI/Docker inspection, `git` history of the bundled blobs,
and the S1 verification described in the plan section (A/B find-module test with
simulated Debian and MySQL layouts, `bash -n` on the regenerated `configure`,
Autoconf 2.69 regeneration). Sources consulted for version/license/packaging
facts are listed below.

S2 verification (this checkout): the guard matrix, the CMake and shell floor
matrices, the find-module probes (header layouts + pkg-config hints), the
regenerated `configure` (one hunk against S1, `bash -n` clean, shell matrix run
from the generated block), the local reconfigure/build (`ninja`, 95/95 steps, no
`FAILED`) and the boot smoke with the new startup line. The pkg-config *executable*
itself is not installed on this Windows host, so the real `pkg_check_modules` call
is exercised only in CI; the hint wiring was verified by supplying the same
variables it sets.

S3 verification (this checkout, working tree): clean configure (MinGW-w64
ucrt64, CMake 4.4.4 + Ninja, GCC 16.2) with the rewritten
`3rdparty/mysql/CMakeLists.txt` and the `FindMYSQL.cmake` prefix hints detects
the MSYS2-installed MariaDB Connector/C 3.4.9 (`Found MYSQL:
C:/msys64/ucrt64/lib/libmysqlclient.dll.a`, `Using MYSQL: ... (MariaDB
Connector/C 3.4.9)`, floor 3.0.8 not triggered). `common` + `char-server`
compile and link against the 3.4.9 headers (161/161 Ninja steps, baseline
warnings only) and the boot smoke loads `libmariadb.dll`, logs `MySQL client
library: 3.4.9` and exits 1 at absent-DB connect. Clang 22.1.8 parses
`sql.cpp` clean against the 3.4.9 headers (`-fsyntax-only`, no errors). Not
executed: no MSVC host (the `.props` wiring is untested), no real-DB
validation; the MSYS2 host has no `pkg-config`, so the `pkg_check_modules`
path stays CI-only as with S2.

Not executed / not claimed: no Linux host, so F1 is documentary + source-code
evidence, not a reproduced failure; no `mariadb_config`/`mysql_config` binary on
this host, so the Autotools floor warning was run from the generated block rather
than through a full `./configure`; no database server available locally, so no part
of T2/T3 has been run; no MSVC host; no CI run (no remote). S1 and S2 changed build
configuration, `src/common/sql.cpp` guards and a single startup log line only.

## 10. Sources

- MariaDB Connector/C overview (latest 3.4.11, LGPL-2.1+, server compatibility,
  `caching_sha2_password` from 3.0.8): https://mariadb.com/docs/connectors/mariadb-connector-c/mariadb-connector-c-guide
- MariaDB Connector/C all releases (3.4.11 dated 2026-10-06):
  https://mariadb.org/connector-c/all-releases/
- Ubuntu noble file lists: `libmariadb-dev`
  (https://packages.ubuntu.com/noble/amd64/libmariadb-dev/filelist) and
  `libmariadb-dev-compat`
  (https://packages.ubuntu.com/noble/amd64/libmariadb-dev-compat/filelist)
- MySQL 8.0 EOL / 8.4 LTS: https://endoflife.date/mysql
- MySQL 8.0.34 automatic-reconnect deprecation (C API docs):
  https://docs.oracle.com/cd/E17952_01/c-api-5.7-en/c-api-auto-reconnect.html
- MySQL Connector/C archived releases (6.1.x era):
  https://downloads.mysql.com/archives/c-c/
