# rAthena — Hoja de ruta de modernización

**Objetivo:** transformar rAthena en un emulador moderno, seguro, eficiente, portable y
mantenible, sin reescribirlo ni alterar sus reglas, scripts, protocolos o persistencia.

**Prioridad:** corrección → compatibilidad → modernización → medición → optimización.

Detalle de auditorías previas (no se repiten aquí): `doc/MODERNIZATION.md`
(toolchain y dependencias) y `doc/MYSQL_MODERNIZATION.md` (etapas MariaDB S1–S5).

## Entorno local

- MinGW-w64 ucrt64: GCC 16.2.0, Clang 22.1.8; CMake 4.4.4 + Ninja
- 4 núcleos; máximo `-j2` (regla permanente); sin Docker
- No disponible en este host: MSVC (la carpeta de VS 2022 está vacía), `pkg-config`,
  servidor MySQL/MariaDB local, host Linux, ARM64

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
- ⏳ S5 — regresión CI + docs: pendiente
- ⏳ Validación con BD real (auth, SQL, transacciones, codificación, persistencia):
  **pendiente — no hay BD local**; requiere entorno con MariaDB (docker-compose
  excluido por reglas)
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

### 2. Modernización C++20/23 — parcial

- ✅ Fixes de compatibilidad: SEH MSVC → bloque plano (`d1a57ee`), `WIN32` desde
  CMake, `NOMINMAX` guardado, compat rapidyaml/httplib
- ✅ Buffer safety en `common` (auditoría + fixes): 25 sitios sprintf/strcpy/
  strcat clasificados; corregidos los overflow-prone (mapindex `getmapname_ext`
  strcpy en buffer estático de 16 B, `mapindex_init` sprintf con `db_path` de
  conf, `checkpath`/`findfile` sin límites); los demás verificados acotados
  (grfio, md5calc, showmsg, socket, LOG_MEMMGR)
- ⏳ Auditoría de `common` restante (ownership ambiguo, macros obsoletas,
  wrappers redundantes) y de `login`/`char`/`map`/tools: pendiente
- ⏳ Evaluación PCH/unity builds/Modules: pendiente (no adoptar automáticamente;
  CMake ya escanea módulos por defecto en C++23+Ninja)

### 3. Arquitectura — pendiente

- ⏳ Ciclos de vida, estado global, errores ignorados, duplicación, acoplamiento,
  estructuras de datos: pendiente
- Candidatos conocidos (no tocar hasta su turno): `Sql_P_KeepaliveTimer` ignora el
  resultado de `Sql_Ping`; estado global tipo `mysql_reconnect_type`

### 4. Fidelidad funcional — parcial

- ✅ Regresión PCRE2 (48 casos chat/preg_match, pinned)
- ⏳ Regresiones login/char/map con BD real: pendiente (sin BD local)

### 5. Benchmarks y profiling — pendiente

- ⏳ Mediciones reproducibles (CPU, memoria, latencias, throughput, asignaciones,
  tiempos de compilación): pendiente

### 6. Optimización de compilación — parcial

- ✅ LTO opt-in (`-DENABLE_LTO`, off por defecto, `check_ipo_supported`)
- ⏳ Experimentos LTO/ThinLTO/PGO/BOLT contra Release baseline: pendientes

### 7. Optimización interna — pendiente

- ⏳ Método obligatorio: hipótesis → benchmark inicial → cambio mínimo → tests →
  medición posterior → conservar/revertir

### 8. Distribución y upstream — parcial

- ✅ Docs: README (cliente BD), `3rdparty/README.txt`, `doc/MYSQL_MODERNIZATION.md`,
  `doc/MODERNIZATION.md`
- ⏳ Doc de builds/distribución, sincronización con upstream, contribuciones
  pequeñas: pendiente

## Tarea actual

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
autorización). La validación real queda explícitamente pendiente hasta que
exista entorno; sin BD, S5 no es ejecutable más allá de la documentación.

**Fase 2 — auditoría C++20/23: en curso.** Buffer safety de `common` cerrada
(ver fase 2); siguen ownership/macros/wrappers en `common` y las demás áreas
(`login`/`char`/`map`/tools), una tarea a la vez.

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
