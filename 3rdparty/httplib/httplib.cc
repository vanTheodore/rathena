// Vendored cpp-httplib v0.60.0 is header-only (single httplib.h).
// This stub TU exists so the CMake static lib and Autotools obj/httplib.a
// keep compiling the header once without changing build wiring.
#if defined(_WIN32) && !defined(NONLS)
// winnls.h (MultiByteToWideChar/CP_UTF8) must be visible while httplib.h
// parses, so <windows.h> precedes it — but <winsock2.h> must come FIRST:
// <windows.h> without WIN32_LEAN_AND_MEAN pulls <winsock.h> (v1), and
// httplib.h's own <winsock2.h> then collides (MSVC C2011 'hostent',
// 'timeval', 'fd_set', 'sockaddr' redefinitions). winsock2 defines
// _WINSOCKAPI_, which blocks windows.h's winsock.h — the canonical
// ordering. NOMINMAX likewise precedes windows.h so its min/max macros
// cannot clobber httplib.h's std::min/std::max (mirrors winapi.hpp).
// web TUs keep the same invariant: src/web/http.hpp includes httplib.h
// before winapi.hpp.
#include <winsock2.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "httplib.h"
