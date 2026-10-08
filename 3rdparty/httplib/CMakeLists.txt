cmake_minimum_required(VERSION 3.13)

# Vendored cpp-httplib v0.60.0 (pinned stable release, 2026-10-07).
# Upstream is now header-only: single httplib.h, no .cc to compile.
# rAthena's split .h + old .cc layout is obsolete; httplib.cc was the old
# split implementation file and is superseded by the self-contained header.
# The historic rAthena httplib.cc stub does `#include "httplib.h"` only so
# that both the CMake static lib and the Autotools obj/httplib.a keep
# working unchanged while compiling the header once.
if ( NOT ENABLE_WEB_SERVER )
    return()
endif( NOT ENABLE_WEB_SERVER )

SET (this_target httplib)
PROJECT(${this_target})

find_path ( HTTPLIB_INCLUDE_DIRS "httplib.h"
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}"
    NO_DEFAULT_PATH )

mark_as_advanced( HTTPLIB_INCLUDE_DIRS )

set ( HTTPLIB_HEADERS
    "${HTTPLIB_INCLUDE_DIRS}/httplib.h"
    CACHE INTERNAL "httplib headers" )

# No obsolete -std=c++11: the project now owns the C++23 floor.
include_directories( ${HTTPLIB_INCLUDE_DIRS} )

ADD_LIBRARY( ${this_target} STATIC "${CMAKE_CURRENT_SOURCE_DIR}/httplib.cc" )
target_include_directories( ${this_target} PUBLIC ${HTTPLIB_INCLUDE_DIRS} )
# Header-only upstream: expose as system include so upstream warnings do not
# drown out rAthena warnings.
target_include_directories( ${this_target} SYSTEM PUBLIC ${HTTPLIB_INCLUDE_DIRS} )

if (NOT WIN32)
    target_compile_definitions(${this_target} PUBLIC "CPPHTTPLIB_SEND_FLAGS=MSG_NOSIGNAL")
endif()
