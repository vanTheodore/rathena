# PR de integración: `deps/mysql-mariadb` → `master`

Revisión pre-integración (2026-10-09, solo lectura salvo docs). Sin rebase,
merge, squash ni push.

## Veredicto: GO (fast-forward, cero conflictos)

- Base: `226fe91` = tip de `master`; la rama va 28 commits por delante y
  `master` 0 por detrás → **merge fast-forward**, sin conflictos por
  construcción. Rama de trabajo intacta.
- Historial: 28 commits coherentes y temáticos (S1–S4 MariaDB, auditorías
  Fase 2/3, fixes verificados, docs, experimentos cerrados). Sin duplicados
  ni cambios ajenos al objetivo.
- Higiene: 96 archivos (+1501/−20421; las bajas son los blobs 6.1.9 y deps
  reemplazadas, ambas intencionales). Cero borrados accidentales fuera de
  `3rdparty/mysql`. Cero binarios/artefactos trackeados. Cero secretos
  (`rathena_test_pw` no aparece en ningún archivo trackeado; el `3307` que
  aparece es documentación y constantes ajenas). Añadidos: `mariadb-
  connector.props`, 4 docs (`MODERNIZATION-ROADMAP`, `MODERNIZATION`,
  `MYSQL_MODERNIZATION`, `BUILD-DISTRIBUTION`) y el CI `modern-toolchain.yml`.
- Contra upstream (`upstream/master`): sin ancestry (snapshot importado sin
  historia común) — no hay merge posible en esa dirección; el port por
  parches sigue siendo la vía documentada. Fuera del alcance de este PR.

## Qué cambia (funcional)

1. **Cliente MariaDB del sistema** (S1–S3): `FindMYSQL.cmake` agnóstico,
   reporte de versión + suelo (≥3.0.8) con warning, log del cliente al
   arrancar, blobs 6.1.9 eliminados, vcxproj vía MSI (`mariadb-connector.props`).
2. **Reconnect explícito** (S4 + 2 fixes): sin `MYSQL_OPT_RECONNECT`;
   reconexión + 1 reintento en `Sql_QueryV/Str`; fallo limpio en transacción;
   reconnect fallido cuenta como 2003 (agotamiento fatal preservado).
3. **ryml locations**: `ParserOptions` con `locations(true)` — sin esto el
   map-server muere al primer warning de BD.
4. **Seguridad buffers**: 11 overflows en map (texto de jugador), 2 en
   common, fix DNSBL `strtok` en login. Resto auditado sin hallazgos.

## Evidencia A/B (MariaDB 11.4.13 portable, 127.0.0.1:3307, BD `ragnarok_test`)

| Caso | Antes | Después |
|---|---|---|
| Boot map con warnings YAML | `terminate ... m_options.locations()` | `Map Server is now online` (1265 mapas, links up) |
| Outage + restart + relogin (type 1) | `Unknown account` con la fila existiendo | `Authentication accepted` |
| KILL de conexiones (drop conductual) | n/a (diseño nuevo) | `Authentication accepted` + lista de char-servers |
| Type 2 tras outage | n/a | fail-closed (ipban deny), sin reintento, servidor vivo |
| Agotamiento (type 1, BD caída) | nunca moría (2002 no contado) | fatal documentado + exit 1 |
| Non-ASCII (`pruebaé1`) | n/a | byte-exacto (`C3A9`) en escritura |
| wait_timeout=60 | n/a | keepalive adaptado (pings /30 s), conexiones vivas |

Stack completo login+char+map verificado con BD real, stderr vacíos.
Compiladores: GCC 16.2 build verde; Clang 22.1.8 `-fsyntax-only` limpio.

## Limitaciones de plataforma (conocidas, no bloquean)

- MSVC sin host (vcxproj actualizados, CI listo). Sanitizers-runtime solo
  Linux-CI. ARM64 solo CI. `configure` regenerado con Autoconf 2.69 pero sin
  re-verificar en host con autoconf. LTO rechazado (crash 0xC0000374),
  ThinLTO sin linker, unity global rechazado (zlib), PGO/BOLT bloqueados.

## Pendiente explícito (no parte de este PR)

T2-2 mail, T2-3 rollback, T2-5 NULL/blob, T2-6 emblema (necesitan cliente de
juego); compat con servidor MySQL (validado solo MariaDB); T3 obsoleto por
decisión S3; T4/CI (necesita push); sync upstream (sin ancestry).

## Riesgos aceptados (heredados, no corregidos aquí)

1. **Reintento no-idempotente tras 2013**: si el servidor ejecutó la
   escritura y después se perdió la respuesta, el reintento la reaplica.
   Idéntico al reconnect transparente anterior; mitigado en transacciones
   (fallo limpio, `in_transaction`). Riesgo preexistente preservado.
2. **Contador `static retry` acumulativo**: nunca se resetea ante éxitos;
   rachas dispersas de fallos acercan el fatal. Preexistente, sin cambios
   (rediseñarlo excede este PR).

## Para integrar (una decisión)

`git checkout master && git merge --ff-only deps/mysql-mariadb`
Nada más: no hay conflictos que resolver ni follow-ups atados al merge.
