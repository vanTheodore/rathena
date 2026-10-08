# MySQL/MariaDB integration audit and modernization plan

Status: **audit only** (branch `deps/mysql-mariadb`, no code changed yet). Everything
below is either executed evidence from this checkout or cited upstream/package
documentation; nothing was validated against a real database server or on Linux/MSVC
(see "Executed vs not executed").

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

### F4 (low) — version guards probe the wrong macro

`src/common/sql.cpp:14,88` gate `my_bool` and `MYSQL_OPT_SSL_MODE` on
`MYSQL_VERSION_ID` (server-version string from the client headers, 50716 in 6.1.9)
instead of client version macros (`LIBMYSQL_VERSION_ID`, MariaDB's
`MARIADB_VERSION_ID`). Works by accident today (the 6.1.9 header does define
`MYSQL_OPT_SSL_MODE`, `mysql.h:176`); will silently misprobe once the client is
replaced. Fix along with the migration.

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

### F8 (info) — client version is invisible at runtime

Nothing logs `mysql_get_client_info()`; diagnosing auth/TLS failures requires
guessing which client is in use. A single startup log line is behavior-neutral and
worth adding during the migration.

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

- **S1 — detection supports the modern packaging** (safe, bounded):
  extend `3rdparty/cmake/FindMYSQL.cmake` to MariaDB locations/names
  (`/usr/include/mariadb`, `libmariadb`, `libmariadbclient`), keep the MySQL names
  as fallback; teach `configure.ac` to fall back to `mariadb_config` and to check
  `mariadb`/`mariadbclient` libs; make CI assert the CMake step actually enabled
  MySQL. Verified locally by simulating the Debian layout (old module fails, new
  module passes) plus `bash -n`/regenerated `configure`.
- **S2 — client-version hygiene**: guard on client version macros, require a
  documented client floor (MariaDB Connector/C ≥ 3.0.8 for `caching_sha2_password`;
  3.4.x recommended), log `mysql_get_client_info()` once at startup.
- **S3 — Windows client replacement**: stop shipping the 6.1.9 blobs; use
  MariaDB Connector/C 3.4.x (installed via MSI, or built from source and vendored —
  decision pending); update `3rdparty/mysql`, the five vcxproj targets and the
  `.sln` wiring; document the required client version in `README.md`.
- **S4 — reconnect/keepalive**: re-implement reconnect explicitly so the
  `mysql_reconnect_type`/`mysql_reconnect_count` contract survives the removal of
  `MYSQL_OPT_RECONNECT`; keep the ping timer.
- **S5 — regression CI + docs**: land T4, restate the supported server/client
  matrix, close this document.

S1 touches no game logic and is verifiable here; S3/S4 depend on the decisions
below and on real-DB validation.

## 8. Open decisions

1. Client standard: MariaDB Connector/C only (recommended) vs "either client".
2. Windows distribution: MSI/system install (documented requirement) vs
   source-built vendored library vs keeping prebuilt blobs (rejected).
3. Whether S1 lands now, before the client decision (it is connector-agnostic).

## 9. Executed vs not executed

Executed in this checkout: full inventory and API-surface extraction
(ripgrep), local CMake cache inspection (which client the Windows build uses),
`LICENSE`/`README`/conf/CI/Docker inspection, `git` history of the bundled blobs.
Sources consulted for version/license/packaging facts are listed below.

Not executed / not claimed: no Linux host, so F1 is documentary + source-code
evidence, not a reproduced failure; no database server available locally, so no
part of T2/T3 has been run; no MSVC host; no CI run (no remote). No code has been
changed yet.

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
