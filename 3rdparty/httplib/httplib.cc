// Vendored cpp-httplib v0.60.0 is header-only (single httplib.h).
// This stub TU exists so the CMake static lib and Autotools obj/httplib.a
// keep compiling the header once without changing build wiring.
#if defined(_WIN32) && !defined(NONLS)
// winnls.h (MultiByteToWideChar/CP_UTF8) must be visible: include the full
// Windows headers here. rAthena's winapi.hpp defines NONLS (which excludes
// winnls.h), so web TUs include httplib.h BEFORE winapi.hpp (see http.hpp).
#include <windows.h>
#endif
#include "httplib.h"
