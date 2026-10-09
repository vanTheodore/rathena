# rAthena — Builds, dependencias, licencias, seguridad y distribución

Fase 8 del roadmap (`doc/MODERNIZATION-ROADMAP.md`). Solo hechos verificados
en este árbol; lo no verificado queda marcado.

## 1. Matriz de build soportada

| Plataforma | Compilador | Estado |
|---|---|---|
| Windows MinGW-w64 ucrt64 | GCC 16.2.0 | ✅ verificado (build completo + boot smoke) |
| Windows MinGW-w64 ucrt64 | Clang 22.1.8 | ✅ verificado (`-fsyntax-only` + baseline previo) |
| Linux x86_64 | GCC 14 / Clang 18 | ✅ vía CI (`modern-toolchain.yml`) |
| Linux AArch64 | GCC (native `ubuntu-24.04-arm`) | ✅ vía CI |
| Windows MSVC (VS 2022 17.8+, `stdcpp23`) | MSVC 19.38+ | ❌ sin host — vcxproj actualizados, CI listo |
| Linux sanitizers (ASan+UBSan) | GCC 14 | ✅ vía CI (MinGW no trae runtimes) |

- CMake mínimo 3.28 (CI puede usar estable reciente); Autotools retenido
  (`configure` regenerado con Autoconf 2.69, sin ediciones manuales).
- Debug/Release. LTO opt-in y **no adoptado** (crash 0xC0000374 al salir —
  ver roadmap fase 6). Unity global rechazado (zlib vendored colisiona).
- Límite local de este host: `-j2` (4 núcleos).

## 2. Dependencias del sistema (cliente MariaDB + resto)

El cliente ya no se vendoriza (etapa S3): instalar el conector del sistema.

| Plataforma | Instalación (evidenciada) |
|---|---|
| Debian/Ubuntu | `sudo apt-get install -y gcc-14 g++-14 cmake ninja-build zlib1g-dev libmariadb-dev libpcre2-dev` (línea del CI) |
| Windows MSVC | MSI oficial "MariaDB Connector/C" 3.4.x (los vcxproj lo localizan vía `3rdparty/mysql/mariadb-connector.props`; `libmariadb.dll` junto a los binarios) |
| Windows MSYS2 ucrt64 | `mingw-w64-ucrt-x86_64-libmariadbclient` (verificado: el host lleva 3.4.9-1) |
| macOS Homebrew | fórmula del conector (soportada en `FindMYSQL.cmake`; sin host para verificar) |

Suelo de cliente: MariaDB Connector/C ≥ 3.0.8 (3.4.x recomendado) o MySQL
Connector/C ≥ 8.0 — ambos build systems avisan por debajo del suelo y el
servidor loguea el cliente enlazado al arrancar.

## 3. Licencias

El proyecto es **GPL-3.0** (`LICENSE` en la raíz — no mover ni relicenciar).

| Dependencia | Licencia | Evidencia |
|---|---|---|
| cpp-httplib 0.60.0 | MIT | cabecera `3rdparty/httplib/httplib.h` |
| nlohmann/json 3.12.0 | MIT | `3rdparty/json/LICENSE.MIT` |
| rapidyaml 0.16.0 | MIT | `3rdparty/rapidyaml/LICENSE.txt` |
| libconfig 1.8.2 | LGPL-2.1+ | `3rdparty/libconfig/LICENSE` |
| zlib 1.3.2 | Zlib | fuente vendored |
| PCRE2 10.49 | BSD-3-Clause WITH PCRE2-exception | `3rdparty/pcre2/LICENCE.md` |
| MariaDB Connector/C (sistema) | LGPL-2.1+ | instalación del sistema, no vendored |
| yaml-cpp 0.9.0 | MIT | licencia upstream verificada (texto MIT de jbeder/yaml-cpp) |

Todas compatibles con GPLv3. GPL, no AGPL: operar un servidor modificado no
obliga a publicar el código (la obligación nace al distribuir).

## 4. Postura de seguridad en flags

Del `CMakeLists.txt` raíz (verificado): Release `-O2` conservador; **no**
`-ffast-math` en lógica de juego/servidor; **no** `-march=native` en
artefactos portables; LTO opt-in off por defecto; sanitizers opt-in
(`-DENABLE_SANITIZERS=address,undefined[,thread]`), nunca en Release.

## 5. Distribución

- Binarios + `LICENSE` + oferta de fuente (el repo público la cumple).
- En Windows, `libmariadb.dll` debe acompañar a los binarios (los vcxproj la
  copian; en MinGW va en el PATH de ucrt64).
- Sin blobs precompilados en el repo (zlib/PCRE2 se compilan de fuente
  vendored; el cliente SQL es del sistema).

## 6. Sincronización con upstream — bloqueada

Este checkout **no tiene remotos configurados** (`git remote -v` vacío) y no
se configura ninguno sin autorización (regla permanente). Sin remoto no se
puede medir divergencia ni preparar rebase. Política cuando exista: ramas
pequeñas por etapa, upstream decide (no se asume adopción de C++23); los
fixes puros (buffers, DNSBL, transacción) son backportables en dialecto viejo
porque ninguno requiere C++23.
