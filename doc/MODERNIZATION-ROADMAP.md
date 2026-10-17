# rAthena — Hoja de ruta de modernización

**Objetivo:** transformar rAthena en un emulador moderno, seguro, eficiente, portable y
mantenible, sin reescribirlo ni alterar sus reglas, scripts, protocolos o persistencia.

**Prioridad:** corrección → compatibilidad → modernización → medición → optimización.

Detalle de auditorías previas (no se repiten aquí): `doc/MODERNIZATION.md`
(toolchain y dependencias) y `doc/MYSQL_MODERNIZATION.md` (etapas MariaDB S1–S5).

## Entorno local

- **Host de sesión 2026-10-17 (sandbox Linux, Debian 12, 2 núcleos, `-j2`):**
  GCC 12.2 (NO sirve para el gate ≥14 — solo para tests standalone);
  **zig 0.17.0 vía pypi `ziglang` = Clang 22.1.8** (compilador de referencia
  del proyecto) con wrappers `/tmp/tools/zig{cc,cxx}`; **CMake 3.28.3 vía
  pypi** (= el cmake de ubuntu-24.04 que usa CI; CMake 4.4.4 también
  configura bien) + Ninja 1.13.2. Deps construidas en `/tmp/deps/prefix`
  (**fuera del repo: no persisten entre sesiones**; receta:
  codeload de `madler/zlib v1.3.2`, `PCRE2Project/pcre2 pcre2-10.49`,
  `openssl-openssl-3.0.17` (`linux-x86_64 no-shared`, libs a `lib64` →
  symlinks en `lib/`), `mariadb-corporation/mariadb-connector-c v3.4.11`
  con `-DWITH_SSL=OPENSSL -DWITH_EXTERNAL_ZLIB=ON -DBUILD_SHARED_LIBS=OFF`
  — `WITH_SSL=OFF` **no compila** en 3.4.11 (la rama de auth sin TLS
  rompe `MA_HASH_CTX`) — luego symlinks `libmariadb.so{,.3}` en `lib/`).
  Configure: `cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_COMPILER=/tmp/tools/zigcc -DCMAKE_CXX_COMPILER=/tmp/tools/zigcxx
  -DCMAKE_PREFIX_PATH=/tmp/deps/prefix -DZLIB_USE_STATIC_LIBS=ON`.
  **Generador Unix Makefiles, no Ninja (local-only):** el Ninja generator de
  CMake ≥3.28 exige `clang-scan-deps` para Clang (reglas `.ddi`/p1689
  generadas aunque no haya módulos); lo trae el paquete de distro de CI
  (clang-18) y zig no lo incluye — probado con un probe mínimo, afecta a
  cualquier Clang sin su `clang-scan-deps`, no al código. sin Docker,
  sin MSVC, sin pkg-config, sin servidor MySQL/MariaDB local, sin ARM64;
  `raw.githubusercontent.com` bloqueado (usar api.github.com/codeload)
- **Host previo (MinGW-w64 ucrt64):** GCC 16.2.0, Clang 22.1.8; CMake 4.4.4
  + Ninja; 4 núcleos; máximo `-j2` (regla permanente); sin Docker; sin MSVC,
  sin `pkg-config`, sin BD local, sin host Linux, sin ARM64

## Estado por fases

### 0. Dependencias — en curso

- ✅ C++23 estricto en CMake (3.28), Autotools y MSVC vcxproj (`stdcpp23`)
  (commits `2935daa`, `fd59e5b`)
- ✅ Deps actualizadas: httplib 0.60.0, json 3.12.0, yaml-cpp 0.9.0, rapidyaml 0.16.0,
  libconfig 1.8.2, zlib 1.3.2, PCRE1→PCRE2 10.49
- ✅ MariaDB S1 (detección agnóstica, `479dede`), S2 (versión + suelo, `75cff02`),
  S3 (cliente del sistema/MSI + blobs 6.1.9 fuera, `68f6ebe`) — verificado en local
  (MinGW GCC 16.2: configure + build + boot smoke; Clang `-fsyntax-only`)
- ✅ S4 (reconnect/keepalive): `MYSQL_OPT_RECONNECT` fuera; reconexión explícita
  en las query paths con el contrato `mysql_reconnect_type`/`count` preservado —
  verificado en local (build 275/275 + boot smoke); validación real pendiente (T2 paso 7)
- ✅ S4 validado con BD real (2026-10-09, MariaDB 11.4.13 portable): recovery,
  KILL/timeout-drop, type 2 fail-closed, agotamiento fatal, keepalive adaptado;
  3 fixes (`a7fd5a0`, `11466f1c`, `5298b5a`). Riesgos heredados aceptados
  (detallados en `doc/INTEGRATION-PR.md`): (1) el reintento tras error 2013
  puede reaplicar una escritura no-idempotente ya ejecutada — idéntico al
  reconnect transparente anterior, mitigado en transacciones (fallo limpio);
  (2) el contador `static retry` del handler nunca se resetea ante éxitos —
  preexistente, sin cambios
- ⏳ S5 — regresión CI + docs: pendiente
- ✅ Validación con BD real (parcial, 2026-10-09): MariaDB 11.4.13 portable en
  127.0.0.1:3307, BD `ragnarok_test` (66 tablas), boot login+char+map con links,
  auth/creación por protocolo, recovery/reconnect/agotamiento/keepalive,
  non-ASCII byte-exacto. Pendiente de cliente de juego: mail, rollback,
  NULL/blob/emblema. Pendiente de entorno: compat MySQL-servidor, T4/CI
- ✅ PCRE2: paridad 48 casos byte-for-byte + `--diag` + CI (`linux-pcre-regress`)
- ✅ Autotools: `configure` regenerado con Autoconf 2.69, sin ediciones manuales
- ❌ MSVC C++23: **pendiente — sin host**; vcxproj actualizados, CI listo
  (`windows-msvc`)

### 1. Estabilidad y calidad — en curso

- ✅ GCC 16.2 + Clang 22.1.8 + C++23 estricto: build completa verde (servers + tools)
- ✅ Sanitizers: `-DENABLE_SANITIZERS` configurable; todos los TU compilan
  instrumentados; runtimes ASan/UBSan **solo en Linux-CI** (MinGW no los trae)
- ✅ Warnings clasificados (~41k diagnósticos GCC: `-Wswitch` ~17k,
  `-Wsign-conversion` ~6k, `-Wpedantic`/`-Wunused-parameter` ~5-6k,
  `-Wconversion` ~4k) — detalle en `doc/MODERNIZATION.md`
- ⏳ Burn-down de warnings + `-Werror` en jobs modernos: pendiente (follow-up grande)
- ⏳ Smoke de arranque con sanitizers: pendiente (Linux-CI)
- ✅ CI: `.github/workflows/modern-toolchain.yml` (linux-gcc, linux-clang,
  linux-pcre-regress, linux-sanitizers, linux-arm64, windows-msvc)
- ✅ **CI roja desde el primer push — diagnosticada y corregida (2026-10-17).**
  Los 10 runs de `modern-toolchain` (2026-10-11 → 2026-10-13) fallaron; solo
  `linux-pcre-regress` fue verde. Causas raíz (los logs completos no se
  pueden descargar en este host — dominio de resultados bloqueado; evidencia:
  anotaciones de la API de jobs + reproducción local exacta):
  (1) **todos los jobs de build** (linux-gcc/clang/sanitizers, windows-msvc):
      el root commit se creó con el patrón `/src/custom` del .gitignore
      activo, así que `git add` omitió los 8 archivos que upstream trackea
      en `src/custom/` (defines_pre/defines_post + 6 `.inc`) — un clone
      fresco no compila (`src/config/core.hpp:12: fatal error: custom/
      defines_pre.hpp`) y `Makefile.in` pierde sus prerequisitos
      `../custom/*.inc`; reproducido localmente con el primer TU;
      (2) **linux-arm64** moría en el paso Configure: `gcc` genérico en
      ubuntu-24.04(-arm) es **GCC 13** (tabla oficial de GCC de Ubuntu;
      gcc-defaults de noble = 13) < piso GCC 14 de CMakeLists.txt; los jobs
      x86 instalan `gcc-14` explícito, el de arm64 no.
  Fixes en esta rama: `be0bab1` (restauración byte-idéntica a upstream
  vía contents API de GitHub; `.gitignore` intacto — upstream también
  ignora `/src/custom` y trackea estos archivos) y `fbcee01` (arm64 espejo
  de x86: `gcc-14 g++-14` + `-DCMAKE_C_COMPILER/-DCMAKE_CXX_COMPILER`).
- ✅ **Primer run verde en gran parte (2026-10-17, run 38997249679,
  tras `be0bab1`+`fbcee01`):** **5/6 jobs SUCCESS** — linux-gcc Release,
  linux-gcc Debug, linux-clang, **linux-arm64 (rojo en los 10 runs
  previos)** y linux-pcre-regress. Rojos restantes: `linux-sanitizers` y
  `windows-msvc`, ambos en «Build all servers + tools» sin log legible
  (descarga de logs bloqueada desde este host; anotaciones = solo exit 1).
  Fix de diagnóstico `f7b7530`: los dos pasos bachean el log y emiten
  las primeras 10 líneas de error como anotaciones `::error` (mismo
  dominio API) — **las anotaciones funcionaron y revelaron las 3 causas
  raíz reales (2026-10-17)**:
  1. **`windows-msvc` a)** `3rdparty/httplib/httplib.cc` incluía
     `<windows.h>` antes que `httplib.h` sin `winsock2`/`NOMINMAX`
     delante → `winsock.h` v1 + `winsock2.h` = C2011 (hostent/timeval/
     fd_set/sockaddr) en MSVC; MinGW lo toleraba (por eso pasó validación
     local). Fix `8f63ed0`: orden canónico `winsock2`→`NOMINMAX`→
     `windows.h` (conservando `winnls.h` para `MultiByteToWideChar`).
  2. **`windows-msvc` b)** `libconfig/scanner.h` (flex) protege su
     `#include <unistd.h>` con `YY_NO_UNISTD_H`, macro que solo define
     `scanner.c` — `libconfig.c` (compilado por minicore/common_base)
     la incluye sin el guard → C1083 en MSVC. Fix `8f63ed0`: la define
     el build de Windows (CMake + los 4 configs del `libconfig.vcxproj`).
  3. **`linux-sanitizers`** `undefined reference to 'typeinfo for
     SkillImpl'` al linkar `csv2yaml`: las 9 virtuals base de
     `SkillImpl` estaban en `skill_impl.cpp` → key function → typeinfo
     solo en un TU que ninguna tool linkea; `map/skill.hpp:322` exige
     el tipo completo (`unique_ptr<const SkillImpl>`) y solo el build
     ASan+UBSan emitía la referencia RTTI. Fix `82e200a`: virtuals base
     inline `// no-op` (sin key function → typeinfo weak en quien lo
     necesite); el include en `yaml.hpp` es obligatorio y queda
     documentado. Validación local: rebuild total + smoke + tools con
     clang+UBSan verdes; el combo exacto GCC14+ASan+UBSan = juez CI
     (repro local imposible: zig sin libasan y gcc12 <14).
  Ciclo de CI con los 3 fixes en curso → **primer resultado (`6b4edea`):
  `linux-sanitizers` ✅ VERDE — el fix `82e200a` pasó el juez exacto
  (GCC14 + ASan + UBSan); los 5 jobs Linux ya eran ✅. `windows-msvc`
  avanzó: los errores de httplib desaparecieron y apareció la siguiente
  capa — (a) `minicore` seguía sin recibir `-DYY_NO_UNISTD_H`
  (target_compile_definitions con string con espacio → `/D` roto en el
  generador VS, mientras `common_base` por COMPILE_FLAGS ya pasaba) y
  (b) `LNK2019 __imp_PathIsRelativeA` (falta `shlwapi.lib`). Fix ronda
  2 = `5e8ef39` (lista `;` en minicore + `#pragma comment(lib)` en
  wincompat.h, precedente de patches locales en ese archivo) — ciclo de
  CI en curso.
- ✅ **CERRADO (2026-10-17): la alternativa (1) aprobada + su segunda
  capa, commit `e3db3f5`.** Dos UBs latentes de upstream en el memory
  manager, expuestos por el `-fsanitize=alignment` por defecto de zig
  0.17: (a) `struct block::data` en offset 36 → cada `unit_head`
  in-block a 4 mod 8 (`block2unit` usa `&data[k*unit_size]` y los
  strides son múltiplos de 16) → fix `alignas(8)` (offset 40;
  `sizeof(struct block)` **igual antes y después: 41000** — 40996 ya
  redondeaba a la alineación del struct) + `static_assert` que pinnea
  el contrato; (b) el canary `FREED_POINTER` en `head+24+size` cae
  desalineado para `size%8≠0` (2.ª capa: panic en `_mstrdup` de un
  nombre de timer de 20 bytes) → los 4 accesos (`*(long*)` ×2 writes,
  ×2 reads en `_mfree`) migran a `canary_put`/`canary_ok` con `memcpy`
  byte-wise (mismos bytes, formato heap intacto).
  **A/B:** repro mínimo zig ±`alignas` = abort 134 vs exit 0
  (`data_off=40`); servidores antes = abort 134 en dos sitios, después
  = los 3 salen exit 1 (convención sin BD) con 0 panics; full rebuild
  limpio = 0 errores, **22.713 warnings vs baseline 22.716 (−3)**;
  tests `ALL CAPPED` + `ALL SORTED` verdes; higiene de warnings de
  `skill_impl` en `5bd45b3` (329 → 0). Juez cruzado = job
  `linux-sanitizers` de CI (GCC no activa `-fsanitize=alignment` por
  defecto). Equivalencia del canary: `memcpy` escribe exactamente los
  mismos 8 bytes que el store anterior.

### 2. Modernización C++20/23 — parcial

- ✅ Fixes de compatibilidad: SEH MSVC → bloque plano (`d1a57ee`), `WIN32` desde
  CMake, `NOMINMAX` guardado, compat rapidyaml/httplib
- ✅ Buffer safety en `common` (auditoría + fixes): 25 sitios sprintf/strcpy/
  strcat clasificados; corregidos los overflow-prone (mapindex `getmapname_ext`
  strcpy en buffer estático de 16 B, `mapindex_init` sprintf con `db_path` de
  conf, `checkpath`/`findfile` sin límites); los demás verificados acotados
  (grfio, md5calc, showmsg, socket, LOG_MEMMGR)
- ✅ Buffer safety + ownership en `login` (auditoría + fix): ownership limpio
  (account db/iter → destroy, nnode list → finalize walk, session_data → capa
  socket; sin new/delete/malloc/free reales). **Bug real corregido**: `strtok`
  destruía `login_config.dnsbl_servs` en sitio — tras el primer login solo se
  consultaba el PRIMER servidor DNSBL de la lista; ahora se tokeniza una copia.
  Además `ip_dnsbl` sprintf → snprintf (token de conf hasta 1023 chars en buffer
  de 256 B). Verificados seguros: `r_ip` (exacto: uint8 máx 15+1=16), `smd5`
  (exacto: loop i<16 en [33]), `message[256]`
- ✅ `char` (auditoría, refute-first): **premisa refutada — sin hallazgos**.
  Buffers seguros: `save_status[128]` (máx 53 acumulado), `t_info[256]` (máx 89,
  en funciones separadas), PIN (`buf[5]` exacto con PINCODE_LENGTH=4),
  nombres de posición/alliance de guild (literales < `NAME_LENGTH`=24). Sin
  expresiones new/delete reales (los "DELETE" son SQL); pairing limpio: flag
  aCalloc sin returns intermedios, fields ×2 con null-check, PIN buf, broadcast
  buf, geoip_cache → final; `int_achievement`/`int_quest` devuelven arrays
  aCalloc'd con ownership documentado («caller to aFree») y los callers lo
  respetan (los paths de break llegan al aFree); el strtok de
  `char_config_split_startpoint` destruye una copia local de la línea de conf,
  no el almacenamiento
- ✅ Buffer safety de `map` (auditoría + fix): 617 sitios clasificados por clase.
  **10 overflows reales corregidos en atcommand.cpp**: sprintf del texto tecleado
  del jugador (%s, hasta 255 chars) con prefijo del formato en el buffer estático
  `atcmd_output[256]` (CHAT_SIZE_MAX) — @me (281 chars), @broadcast/
  @localbroadcast (281), @request, unknown-command, unknown-permission ×2,
  unknown color ×2, unknown-costume → snprintf. **1 en channel.cpp**: `mout[60]`
  desbordaba incluso con nombres por defecto (formato 37 + canal ≤20 + mapa ≤15
  = 73 > 60) → snprintf. Verificados seguros: script.cpp (codegen interno con
  formatos literales), pc/duel/skill/chrif (nombres/números acotados), y el path
  del chat en clif.cpp (mensaje acotado a ≤255 en `clif_process_message:10532`,
  `output[304]` cabe el máximo 282, `fakename`/`out_name` ambos NAME_LENGTH con
  escritura acotada en @fakename)
- ✅ Ownership de `map` (auditoría, refute-first): **premisa refutada — sin
  bugs**. Cero expresiones new/delete/malloc/free reales (los ~100 matches eran
  comentarios y strings SQL). **Placement new ×6** (map_session_data ×3,
  mob_data, guardian_data, pet_data) emparejado con **dtors explícitos** en el
  dispatch de destrucción de bloques (`map_destroyblock`: BL_MOB/PET/HOM/MER/
  ITEM/SKILL/NPC/CHAT/ELEM llaman a su dtor + aFree); el dtor de
  `map_session_data` se difiere a `chrif_auth_delete` (runtime) y
  `auth_db_final` (shutdown) — documentado en el código, ambos sitios
  verificados correctos (dtor + aFree dentro de `if(node->sd)`, paths
  mutuamente excluyentes — un aparente use-after-free era artefacto de mi grep
  que fusionó dos funciones); el reciclaje (`map_freeblock`) tiene un único
  path de destrucción; los arrays aCalloc'd de int_achievement/int_quest con
  ownership documentado
- ✅ Ownership en `common` (auditoría, refute-first): **premisa refutada — no hay
  bugs**. Cero expresiones `new`/`delete` reales fuera de `utilities.cpp`
  (`unique_ptr` RAII + `levenshtein` new[]/delete[] emparejado); `malloc/free`
  crudos de `core.cpp` emparejados sin returns intermedios (inconsistencia de
  estilo sin valor: saltan el memory manager pero se liberan); `aMalloc`/
  `CREATE` con teardown centralizado consistente verificado en sql/grfio/
  socket/timer (grfio_reads: bloques de asignación mutuamente excluyentes, las
  tres rutas de error liberan); sin leaks ni double-frees en las rutas
  auditadas. LSan en CI pendiente (detección empírica)
- ✅ Macros en `common` (auditoría): `TRUE`/`FALSE` (84 usos) — renombrar a
  bool/true sería un cambio cosmético masivo, **excluido por reglas**; el
  bandaid `my_bool` es necesario (MySQL 8); `CREATE`/`aMalloc` son la gestión
  de memoria propia del proyecto
- ✅ `tools` (auditoría, refute-first): **premisa refutada — sin hallazgos**.
  Los 2 sprintf de `mapcache.cpp` son seguros (nombres acotados con
  `sscanf("%15s")` en buffer de 256 B; máx 25 bytes); cero new/delete/malloc/
  free reales; el pairing de `aMalloc` correcto (`gat` liberado en la ruta
  temprana y la normal, `m->cells`/`write_buf` al final); csv2yaml/yaml2sql/
  yamlupgrade sin hallazgos
- ✅ `apply_rate`/`apply_rate2` (1.ª modernización justificada,
  `modernize/apply-rate-template`): macros de triple evaluación → function
  templates `constexpr` con `std::common_type` explícito (evalúan 1 vez,
  cero warnings nuevos). Test permanente `tools/apply-rate-check.cpp`
  (convención pcre-regress: standalone, sin cablear al build): rejilla de
  equivalencia macro-vs-template (bordes, negativos, mixtos exactos de los
  24 sitios, double) + control negativo (la macro legacy evalúa 2×, el
  template 1×) + `static_assert`s; verde en GCC 16.2 y Clang 22.1.8.
  Revisión semántica por sitio (tipos de retorno, promociones, negativos,
  overflow): sin diferencias reales — veredicto GO. 24 call sites intactos
- ✅ **`cap_value` (2026-10-17, `3019b9a`, iteración elegida)**: los 498
  call sites migrados SIN editarlos (cuerpo del template = ternario
  literal de la macro → mismas conversiones y mismo tipo de resultado).
  Auditoría: 0 usos lvalue/`&`/`sizeof` del resultado, 0 argumentos con
  efectos laterales, 0 llamadas multi-línea, 0 sitios en headers, 0 `.c`;
  inventario de funciones en argumentos (atoi ×13, pc_maxparameter ×12,
  apply_rate ×6, stoi ×6, script_getnum64, getSCE, consultas puras) —
  todo lectura/query ⇒ evaluación única preserva todo resultado observable
  (y elimina la trampa de triple evaluación). Test
  `tools/cap-value-check.cpp` con el header REAL: rejilla legacy-vs-template
  de las formas de call site + igualdad de tipo + `static_assert` constexpr
  + control negativo legacy=3 evals / template=1. Verde GCC 12.2 y
  Clang 22.1.8 con la suite de warnings; rebuild completo exit 0 (líneas
  de warning atribuidas a cap_value 687→249, clase preexistente) + boot
  smoke verde
- ✅ `rand %100` ×4 en battle → `rnd_chance()` (`modernize/rnd-chance-battle`):
  elimina RNG libc sin seed (secuencia idéntica por boot), salida débil y
  sesgo de módulo; probabilidades preservadas (bordes 0/≥100 exactos,
  int32 firmado como la comparación original); test permanente
  `tools/rnd-chance-check.cpp` verde en GCC/Clang; build sin warnings nuevos
- ⏳ Backlog priorizado (una por iteración, con test de equivalencia):
  ~~`qsort` ×2 → `std::sort`~~ ✅ **cerrado (2026-10-17)**: script.cpp ya
  estaba (`tools/script-sort-check.cpp`); el último —`storage.cpp`—
  migrado en `9619735` con test permanente
  `tools/storage-sort-check.cpp` (acuerdos legacy/moderno en el dominio
  consistente + contrato de orden con slots vacíos + 2 controles
  negativos: inconsistencia legacy both-way y overflow de resta en ids
  2^31 aparte; verde GCC 12.2 y Clang 22.1.8). ~~`cap_value`~~ ✅ ver la
  entrada de arriba. ~~`ARR_FIND`~~ ✅ **cerrado (`efdc506`)**: template
  `arr_find` + shim que liga `__var` por referencia (los 254 sitios
  reales compilan sin editar; auditoría previa: todos 4 args, `__cmp`
  siempre referencia la variable, bounds puros, sin colisiones); contrato
  idéntico salvo bounds evaluados 1× en vez de N+1 (control del test
  `legacy=5 modern=1`, seguro porque todo bound es puro); test
  `tools/arr-find-check.cpp` verde GCC 12.2/Clang 22.1.8; rebuild 0
  errores. (Corrección de medición: los deltas por iteración que
  comparaban logs parciales de 101–116 TUs recompilados contra full
  no eran como-contra-como; el único full limpio comparable es
  22.713 → ver cierre del lote abajo.) ~~`ARR_MOVE`~~ ✅ **cerrado (`9f54a03`)**:
  shim sobre `std::rotate` para los 2 sitios reales (char_mapif,
  `fame_list` POD); resultado byte-idéntico al legacy de 3 `memmove`
  (grid A/B + layouts esperados), ahora constexpr y bien definido para
  tipos no triviales (pin con `std::string`); `ARR_MOVERIGHT/LEFT` sin
  call sites se mantienen como API upstream; test
  `tools/arr-move-check.cpp` verde GCC/Clang. ~~`atoi`~~ ✅ **cerrado
  (`e29a68f`)**: `a2i()` en strlib.hpp = `(int)strtol` + saturación a
  `[INT_MIN, INT_MAX]` — paridad exacta con libc `atoi` en rango
  (34/34) y, fuera de rango, el resultado Windows sobre todas las
  plataformas (glibc hoy devuelve −1/0/cast-wrap: divergencia
  Windows/Linux real sobre el mismo input); `nullptr → 0`. Migración
  mecanizada con guardas (comentarios/strings/`BUILDIN_*`/`src/custom`
  verbatim): **707 sitios → 0 restantes**, 41 archivos, diff
  línea-equilibrado; `timer.cpp` único TU sin el header (lo resolvió el
  compilador, `-k` lo probó); test `tools/a2i-check.cpp` verde
  GCC/Clang sin warnings. ~~`std::stoi`/`std::stoul`~~ ✅ **cerrado
  (`6f3d5be`)**: 167 + 2 sitios migrados a `a2i` (overload
  `std::string`; `<string>` en strlib). Diagnóstico: `std::stoi` lanza
  sin ningún `catch` en los 9 TUs afectados (controllers web parsean
  parámetros HTTP con esto) → garbage/corrupt = `std::terminate`;
  paridad exacta en rango (34/34) y 7 inputs de clase
  throw-and-terminate anclados en el test. Verificación de args
  multi-`pos`: 0 reales (los 35 del heuristic eran comas anidadas).
  Tras la migración, 0 llamadas `std::sto*` en posición de código en
  `src/`. ~~`atol`~~ ✅ **cerrado (`e4c6a10`)**: 10 sitios → `a2l()`
  (guard de nullptr; todos los sites se alimentan de `Sql_GetData`,
  que devuelve `nullptr` sin fila → `atol(NULL)` = segfault real hoy en
  rutas de login/char); valores idénticos a `atol` en todo lo que
  sobrevivía antes (paridad 15/15 anclada, `long` de ancho de plataforma
  intacto para los casts `(uint32)` circundantes); el 11º token es el
  comentario doc. **`atof` (3 sitios) queda pendiente de decisión**:
  sus sitios interesantes alimentan casts a `uint32` donde el clamp del
  rango float exige decidir tipo destino (no hacer a ciegas). **Medición honesta del lote (full limpio 194 TUs): 22.752
  warnings vs 22.713 del full limpio de alineación = +39 netos, 0
  atribuidos a `a2i`/strlib, delta por archivo migrado ≈ 0 (mob +2 de
  atribución).** Lección: los deltas −359/−727 de iteraciones previas
  (commits `efdc506`/`e29a68f`, texto histórico no reescribible)
  comparaban logs parciales — desde ahora solo full-limpio-contra-full-
  limpio. **Siguiente del backlog:**
  `expected`, `string_view`/`span` (aparcados por alcance);
  `NULL→nullptr` y `to_underlying` rechazados (estética)
- ✅ `StringBuf` (diseño evaluado, **se mantiene**): ~494 usos
  (common/char/map). Ya es RAII (ctor/dtor) y su valor frente a `std::string`
  es la integración con el memory manager (aMalloc2/aRealloc2 con ALC_MARK →
  contabilidad y detección de leaks por sitio); migrar perdería eso por cero
  ganancia funcional. Solo 51 sitios con ownership manual
  (Malloc/Init/Free); el resto son miembros por valor. Alternativas
  descartadas: reemplazo total por `std::string` (transversal, ~500 sitios,
  pierde tracking), abstracción nueva (innecesaria). Sin cambios
- ⏳ Auditoría de `char`/`map`/tools hecha; `StringBuf` cerrado (se mantiene);
  resta PCH/unity/Modules en la fase
- ✅ **Unity builds evaluados (2026-10-09): rechazado global, sin adopción**.
  `-DCMAKE_UNITY_BUILD=ON` global rompe en 52 s en el primer batch:
  `3rdparty/zlib/.../Unity/unity_1_c.c.obj` — los headers internos de zlib
  (`inftrees.h`, `inflate.h`: structs/enums anónimos, enumeradores
  CODES/LENS/DISTS/HEAD) no están hechos para amalgamación y colisionan al
  fusionar TUs. Es el 3rdparty, no el código propio. Variante acotada
  (unity solo en targets propios, OFF en 3rdparty) anotada como follow-up —
  requiere tocar el glue de build de 3rdparty y otro build completo; no
  quemado aquí. Configure del experimento: 573 s (anómalo frente a ~99 s;
  causa no investigada)

### 3. Arquitectura — pendiente

- ⏳ Ciclos de vida, estado global, errores ignorados, duplicación, acoplamiento,
  estructuras de datos: pendiente
- Candidatos conocidos (no tocar hasta su turno): `Sql_P_KeepaliveTimer` ignora el
  resultado de `Sql_Ping`; estado global tipo `mysql_reconnect_type`

### 4. Fidelidad funcional — parcial

- ✅ Regresión PCRE2 (48 casos chat/preg_match, pinned)
- ⏳ Regresiones login/char/map con BD real: pendiente (sin BD local)

### 5. Benchmarks y profiling — parcial

- ✅ **Baseline de compilación** (MinGW-w64 ucrt64 GCC 16.2, CMake 4.4.4 +
  Ninja, Release, `-j2`, build completo tras tocar todos los fuentes):
  **772 s (~12,9 min) el 2026-10-09 y 920 s (~15,3 min) en repetición** —
  varianza de máquina ±20% con una medición por lado; el rango honesto es
  13–15 min. Warnings del build completo: ~40,5k (coherente con los ~41k
  documentados en `doc/MODERNIZATION.md`). Binarios (deps estáticas,
  1.ª medición): map-server 9,0 MB, csv2yaml 2,0 MB, web-server 2,0 MB,
  yamlupgrade 1,9 MB, yaml2sql 1,6 MB, char-server 1,2 MB, login-server
  0,8 MB, mapcache 0,7 MB. La repetición con fuentes idénticas dio binarios
  ~2% más pequeños (map-server 9,0→8,8 MB) — causa no investigada (candidato:
  no-determinismo del enlazado); sin impacto funcional (exit 0 + smoke
  limpio). Los binarios LTO con crash quedaron reemplazados por los limpios
- ⏳ CPU/memoria/latencias/throughput/asignaciones de los servidores:
  **bloqueado** — necesita un servidor corriendo (BD real; docker-compose
  excluido por reglas)
- ⏳ Profiling de map-server, timers, pathfinding, IA, scripts, combate, SQL y
  red: bloqueado (igual)
- ⏳ Repetir la medición del baseline para verificar reproducibilidad:
  pendiente

### 6. Optimización de compilación — parcial

- ✅ LTO opt-in (`-DENABLE_LTO`, off por defecto, `check_ipo_supported`)
- ✅ **Experimento LTO A/B (medido 2026-10-09, GCC 16.2, -j2)**: LTO soportado
  (configure 99 s); build completo **613 s vs 772 s baseline** (una medición
  por lado — la varianza de máquina puede explicar parte; repetir para
  rigor). Tamaños: **-11% a -56% en todo** (map-server 9,0→8,3 MB,
  char-server -22%, login-server -50%, mapcache -56%). **PERO: crash
  reproducible al salir (exit -1073740940 = 0xC0000374, heap corruption) en
  map-server y char-server** — el mismo código sin LTO sale limpio (exit 1).
  **Decisión: LTO NO se adopta** (regla: solo mejoras sin regresiones; sigue
  off por defecto). El crash es un candidato real de UB que el build normal
  enmascara — investigar con los sanitizers de CI o un debugger (no hay gdb
  ni eventos WER en este host)
- ⏳ Experimentos PGO/BOLT contra Release baseline: **bloqueados** — PGO
  necesita ejecutar binarios instrumentados con carga real (servidor corriendo
  → BD real); BOLT necesita Linux + perf (sin host Linux)
- ✅ **ThinLTO descartado en este host (2026-10-09, evidencia directa)**:
  Clang 22.1.8 ucrt64 no trae `ld.lld`/`lld` (`-fuse-ld=lld` inválido) y el
  `ld` GNU falla cargando el plugin `LLVMgold.dll` — sin linker compatible
  no hay ThinLTO. Verificación: hello-world `-flto=thin`, segundos, sin build
  completo
- ⏳ Investigación del crash LTO (UB expuesto vs miscompile): pendiente —
  requiere debugger o sanitizers en CI

### 7. Optimización interna — pendiente

- ⏳ Método obligatorio: hipótesis → benchmark inicial → cambio mínimo → tests →
  medición posterior → conservar/revertir

### 8. Distribución y upstream — parcial

- ✅ Docs: README (cliente BD), `3rdparty/README.txt`, `doc/MYSQL_MODERNIZATION.md`,
  `doc/MODERNIZATION.md`
- ✅ `doc/BUILD-DISTRIBUTION.md` (nuevo): matriz de build verificada, instalación
  de dependencias por plataforma (evidenciada: apt del CI, MSI en Windows,
  `mingw-w64-ucrt-x86_64-libmariadbclient` en MSYS2), tabla de licencias
  (yaml-cpp sin verificar — sin archivo en el árbol), postura de flags,
  distribución GPL-3.0
- ⏳ Sincronización con upstream: **bloqueada** — sin remotos configurados (no
  se configura ninguno sin autorización); contribuciones pequeñas pendientes
  de decisión (los fixes son backportables en dialecto viejo)

## Tarea actual

**Lote backlog 2026-10-17 (Gogogo — tres iteraciones de familia):**
`efdc506` (ARR_FIND template+shim, 254 sitios sin editar), `9f54a03`
(ARR_MOVE via `std::rotate`, 2 sitios), `e29a68f` (`a2i` + migración de
707 `atoi`), `6f3d5be` (`stoi`/`stoul` → `a2i`, 169 sitios). Validación del lote: full rebuild 0 errores, **21 625
warnings** (−727 vs 22 352; burn-down acumulado 22 716 → 21 625);
suite de regresión completa verde (cap-value, storage-sort,
script-sort, apply-rate, rnd-chance, arr-find ×2, arr-move ×2, a2i ×2)
y boot smoke ×3 servidores = exit 1 sin panics. **Totales corregidos
(medición):** solo el full limpio es comparable — 22.713 (alineación)
→ **22.754 (cierre del lote de 5 iteraciones, 194 TUs)** = +41 netos
con 0 warnings atribuidos a `a2i`/`a2l`/strlib (per-archivo migrado ≈ 0);
los "−359/−727" previos eran parciales (101–116 TUs) y quedan
rectificados en la entrada de backlog. Iteraciones 4ª y 5ª: `6f3d5be`
(`stoi`/`stoul` → `a2i`, 169 sitios, controllers web sin-terminate) y
`e4c6a10` (`atol` → `a2l`, 10 sitios, crash `Sql_GetData` NULL).
**CI del lote — todos los ciclos 7/7 SUCCESS en ambos runs:**
`9f54a03` (ARR_FIND+ARR_MOVE), `018ebdc` (a2i+docs; jobs verificados
14/14), `13854a3` (stoi+corrección docs), `e4c6a10` (a2l; jobs
verificados 14/14 — windows-msvc, gcc Debug+Release, clang, arm64,
pcre-regress, sanitizers).

**Cierre de sesión 2026-10-17 (host Linux sandbox — ver «Entorno local»):**
3 commits en la rama (`be0bab1`, `fbcee01`, `192cc2f`):

1. **`fix(build)`: restauración de `src/custom/` (8 archivos).**
   Diagnóstico: los 10 runs de CI rojos + clone fresco no-compilable;
   evidencia: `GET /repos/vanTheodore/rathena/contents/src/custom` → 404,
   reproduce local del fatal error en el primer TU, tamaños verificados
   contra el contents API de upstream. Cambios: 8 archivos byte-idénticos
   a `rathena/rathena` master. Pruebas: ver (3). Limitaciones: `windows-msvc`
   asume la misma causa — confirmar en el próximo run.
2. **`ci(arm64)`: GCC 14 explícito.** Mecanismo: gcc-defaults de noble = 13
   (tabla oficial Ubuntu) vs gate ≥14; evidencia: paso fallido = Configure
   en arm64 vs Build en x86 en el run 38482270991. Limitación: sin host
   ARM64 ni run de CI aún — el fix es espejo exacto del job x86 verde en
   configure.
3. **`modernize(storage)`: último `qsort` → `std::sort`.** Racional de
   corrección: el comparador legacy **no era orden débil estricto** (dos
   slots vacíos → >0 en ambos sentidos; reutilizarlo en `std::sort` es UB)
   y su resta con signo invertía ids a 2^31. Tests: `tools/storage-sort-check`
   verde en GCC 12.2 y Clang 22.1.8 (equivalencia legacy/moderno donde
   legacy era consistente, contrato de orden, 2 controles negativos que
   se disparan como se espera).

**Validación local de CI completa (2026-10-17, réplica del job
`linux-gcc` Release con Clang 22.1.8/CMake 3.28.3):** configure con TODAS
las aserciones de CI en verde (`Enabled PCRE code`, `Configuring for system
MYSQL - done`, `Found MYSQL: .*mariadb` → 3.4.11, `Using MYSQL: ...
(MariaDB Connector/C `, sin `client floor`); **build completo 100% exit 0**
(0 errores; 22 716 warnings Clang — baseline de la suite de warnings, el
burn-down sigue siendo follow-up); todos los binarios generados; **boot
smoke** de map-server = exit 1 esperado (sin BD) + línea
`MySQL client library: 3.4.11` (grep de CI ✓); regresión PCRE2 local =
48 casos byte-for-byte vs baseline PCRE 8.33 con PCRE2 10.49 ✓ (sin ASan;
la variante ASan es de CI). Limitaciones: no es GCC ni MSVC ni arm64
(hosts/compilers ausentes), sin BD real (smoke = el mismo nivel que CI),
`cmake -G Ninja` local requiere `clang-scan-deps` (ver Entorno local).
**Pendiente de autorización:** push de la rama (dispara el CI en todos los
jobs) y decisión de PR.

**Deuda registrada (2026-10-11, slices SQL UAF+retry integrados):**
- `tools/proto-smoke.ps1`, prueba `2b-char-list`: RESUELTO en
  `modernize/smoke-positive` — verifica contenido vía `ParseCharList`
  (nombre esperado en `0x6b`), no solo el ID.
- Creación de personaje (`char_make_new_char`, `src/char/char.cpp`): los
  rechazos previos al INSERT (`slot inválido`, `límite de cuenta`, `-1`/`-2`/`-4`)
  son silenciosos (sin `ShowWarning`/`ShowDebug`); un fallo de validación es
  indistinguible en logs de un fallo de BD. Logging mínimo pendiente (sin
  cambiar la lógica).

**Deuda registrada (2026-10-11, migración login a InnoDB):**
- Candidato a bug independiente (NO corregido en la migración): el retry de
  `web_auth_token` (`src/login/account.cpp`, `mmo_auth_tosql`) condiciona el
  ciclo a error 1062 pero nunca incrementa `i`, por lo que ante 1062
  persistente no termina en `MAX_RETRIES` (y el aviso de `i == MAX_RETRIES`
  es inalcanzable). Colisión real casi imposible (token aleatorio), pero el
  contador muerto debe revisarse en un slice propio.

**Fase 6 — experimento LTO A/B (cerrado): no se adopta** (crash reproducible
0xC0000374 al salir en map/char; tamaños -11%/-56% pero con regresión —
ver fase 6). Siguiente accionable sin BD: repetir el baseline de Fase 5 para
rigurosidad, o ThinLTO/PGO/BOLT contra baseline; los benchmarks de servidor
siguen bloqueados sin BD real.

**Fase 3 — arquitectura: errores ignorados (cerrado, refute-first).** El sweep
de llamadas SQL sin comprobar resultado: 9 sitios en todo el codebase, todos
intencionales — 5 en realidad comprobados (la primera línea de `if` de dos
líneas: buyingstore/vending close+final con `Sql_ShowDebug`), 1 stat no
crítica (char moves) y 3 `ROLLBACK` fire-and-forget en paths de error (el
error original ya se loguea; un ROLLBACK fallido sobre conexión muerta es
inofensivo — el servidor revierte las transacciones no comprometidas al
desconectar). El keepalive que ignora el resultado de `Sql_Ping`: inofensivo
con el diseño S4 (la siguiente statement reconecta). **Sin fallos de
persistencia silenciosos.**

**Fase 3 — arquitectura: todas las categorías auditadas.**

- ✅ **Errores ignorados** (refutado): 9 sitios SQL sin comprobar, todos
  intencionales (5 comprobados en if de dos líneas, 1 stat no crítica, 3
  ROLLBACK fire-and-forget con el error original logueado); keepalive sin
  comprobar `Sql_Ping`: inofensivo con S4. Sin fallos de persistencia
  silenciosos
- ✅ **Interacción transacción/reconnect** (el hallazgo material, arreglado con
  la opción B — fallo limpio, commit `4350e0c`)
- ✅ **Estado global** (refutado como problema): cero threading (sin
  `std::thread`/`mutex`/`pthread_create` en ningún servidor) — modelo
  single-thread confirmado; los globals (config structs, contadores) son el
  diseño y no pueden correr
- ✅ **Duplicación** (refutada): los saves de items están unificados en
  `char_memitemdata_to_sql` con tableswitch (inventory/cart/storage); los
  patrones per-entity de `int_*` son por diseño (tablas distintas); el
  boilerplate de atcommand (msg_txt + output) es por diseño
- ✅ **Ciclos de vida** (verificados en las auditorías de ownership):
  creación/destrucción emparejadas (placement new + dtors explícitos),
  teardown centralizado (Sql_Free, grfio_final, socket/timer final)
- ✅ **Acoplamiento/dependencias** (limpias post-Fase 0): cliente del sistema/
  MSI, cero blobs vendidos; el protocolo inter-server es el diseño
- ✅ **Estructuras de datos**: sin problemas demostrados (strdb/DBMap y los
  vectores de map_session_data son el diseño) — sin cambios por reglas
  («solo ante problemas demostrados»)

**S5 — regresión CI + docs (Fase 0, última etapa MariaDB).** Alcance (según
`doc/MYSQL_MODERNIZATION.md`):

- **T2** suite de conformidad con BD real: 8 escenarios (cuenta/personaje/
  posición/inventario/zeny, mail con adjunto — transacción, save con fallo
  forzado — rollback, round-trip no-ASCII, NULL/boundary en queries y prepared
  statements, blob de emblema, **reconexión type 1 y 2 con outage** — la
  validación pendiente de S4 — y la ruta wait_timeout con timeout corto)
- **T3** comparación de clientes: T2 con el cliente 6.1.9 (control Windows) y el
  nuevo — estado de BD idéntico
- **T4** CI: job Linux que instala MariaDB server, importa `sql-files` y corre T2
  headless; job Windows con build del cliente elegido

Bloqueo: T2/T3 requieren un servidor MariaDB real (no hay BD local; Docker
excluido por reglas) y T4 requiere ejecutar CI (sin push/remotos sin
autorización). Investigación de desbloqueo (2026-10-09, solo lectura): sin
servidor/servicio/proceso en este host (sin servicios mysql/maria, sin
rutas de instalación, nada en el puerto 3306; MSYS2 solo tiene el cliente
3.4.9-1 y `mariadb-clients` en repos — sin paquete de servidor). Vía viable:
ZIP portable de MariaDB en carpeta aislada + puerto 3307 (pasos documentados
en el informe, sin instalar nada). Autoconf y gdb tampoco están instalados.

**Validación real ejecutada (2026-10-09, MariaDB 11.4.13 portable en
127.0.0.1:3307, BD `ragnarok_test`, 66 tablas):** boot completo login+char+map
con enlaces inter-server (1265 mapas, castillos, clanes); creación de cuenta
y login por protocolo real; recovery type 1 tras outage (ACCEPT); KILL de
conexión = recovery (ACCEPT, T2-7/T2-8 conductual); type 2 fail-closed sin
reintento y servidor vivo; agotamiento type 1 = shutdown fatal documentado
(exit 1); round-trip non-ASCII byte-exacto (é = C3A9); keepalive adaptado a
wait_timeout=60 (pings cada 30 s). Tres fixes que salieron de aquí:
ryml-locations (`a7fd5a0`), retry con statement preservado (`11466f1c`) y
conteo 2003 del reconnect fallido (`5298b5a`). T3 obsoleto (cliente 6.1.9
eliminado por decisión S3); mail/rollback/account-save/NULL/blob/emblem
pendientes de cliente de juego; compat MySQL-servidor pendiente (esto valida
MariaDB solamente).

**Fase 2 — auditoría C++20/23: todas las áreas auditadas.** `common` cerrado
(buffer safety ✅, ownership ✅ refutado, macros ✅), `login` cerrado (bug DNSBL
de strtok corregido), `char` cerrado (premisa refutada), `map` cerrado (buffer
safety: 11 overflows corregidos; ownership: refutado) y `tools` cerrado
(premisa refutada). Restante en la fase: la
evaluación PCH/unity/Modules. Siguiente fase del roadmap: **3 (arquitectura)**.

## Reglas permanentes

- Una tarea a la vez; enfoque refute-first; commits pequeños e independientes;
  tests A/B donde correspondan
- Máximo `-j2`; sin Docker; sin push/merge/remotos sin autorización
- Compatibilidad Windows/Linux; sin abstracciones innecesarias ni cambios
  cosméticos masivos
- «Compila» ≠ «funciona»; no inventar resultados de CI/benchmarks/pruebas
- No mezclar modernización técnica con cambios de reglas
- Refactor arquitectural grande → detenerse y presentar alternativas antes de
  ejecutarlo
- Cerrar cada tarea: diagnóstico, evidencia, cambios, pruebas ejecutadas,
  limitaciones y commit

## Definition of Done

rAthena compila y funciona verificablemente en sus plataformas soportadas,
dependencias modernizadas, regresiones suficientes, C++23 más seguro y mantenible,
deuda crítica resuelta o documentada, optimizaciones respaldadas por benchmarks.
Las plataformas o pruebas inaccesibles quedan explícitamente pendientes.
