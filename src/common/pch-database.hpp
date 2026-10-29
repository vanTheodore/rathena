#pragma once

// Los targets mixtos C/C++ (p.ej. libconfig dentro de minicore) generan
// también su par en C: el contenido solo existe para C++.
#ifdef __cplusplus

// PCH extra de ryml/YAML — solo con -DENABLE_PCH=ON, adjuntado por
// target a los objetivos que cargan base de datos en YAML
// (map/char + herramientas yaml; ver doc/MODERNIZATION-ROADMAP.md §5).
// Aporta la cadena database.hpp → ryml (≈30 s de parseo × decenas de TUs
// en el perfil 2026-10-17); los TUs que no la usan solo pagan la carga
// del AST ya compilado, que es despreciable frente al re-parseo.

#include <common/database.hpp>
#endif /* __cplusplus */
