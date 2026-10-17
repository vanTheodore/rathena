#pragma once

// Los targets mixtos C/C++ (p.ej. libconfig dentro de minicore) generan
// también su par en C: el contenido solo existe para C++.
#ifdef __cplusplus

// Precompiled header base de rAthena — solo se usa con -DENABLE_PCH=ON
// (CMake target_precompile_headers, adjunto a rathena_cxx23).
//
// Orden justificado con el perfilado -ftime-trace del build completo
// (ver doc/MODERNIZATION-ROADMAP.md §5): macros de configuración primero,
// después la cadena común de headers propios. Cada header incluye sus
// propias dependencias std; no se listan a mano para que el orden textual
// sea siempre el de los propios headers.
//
// No incluye database.hpp/ryml (solo lo consumen ~la mitad de los targets):
// esos targets adjuntan pch-database.hpp aparte.

#include <config/core.hpp>
#include <common/cbasetypes.hpp>
#include <common/mmo.hpp>
#include <common/core.hpp>
#include <common/utilities.hpp>
#include <common/strlib.hpp>
#include <common/showmsg.hpp>
#include <common/malloc.hpp>
#endif /* __cplusplus */
