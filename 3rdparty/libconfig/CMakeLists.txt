
find_path( LIBCONFIG_INCLUDE_DIRS "libconfig.h"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}"
	NO_DEFAULT_PATH )
find_path( LIBCONFIG_SOURCE_DIR "libconfig.c"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}"
	NO_DEFAULT_PATH )
mark_as_advanced( LIBCONFIG_INCLUDE_DIRS )
mark_as_advanced( LIBCONFIG_SOURCE_DIR )

set( LIBCONFIG_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/grammar.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/libconfig.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/parsectx.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/scanctx.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/scanner.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/strbuf.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/strvec.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/util.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/wincompat.h"
	CACHE INTERNAL "libconfig headers" )
set( LIBCONFIG_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/grammar.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/libconfig.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/scanctx.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/scanner.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/strbuf.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/strvec.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/util.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/wincompat.c"
	CACHE INTERNAL "libconfig sources" )
set( LIBCONFIG_DEFINITIONS
	"-DLIBCONFIG_STATIC"
	CACHE INTERNAL "libconfig definitions" )

if( WIN32 )
	# scanner.h (flex) wraps its #include <unistd.h> in
	# `#ifndef YY_NO_UNISTD_H`, but only scanner.c defines the macro;
	# libconfig.c also includes scanner.h and MSVC has no <unistd.h>
	# (C1083 at scanner.h:460 in minicore/common_base). The scanner
	# needs nothing from unistd on Windows; MinGW ships the header but
	# takes the same path so both Windows toolchains agree.
	set( LIBCONFIG_DEFINITIONS "${LIBCONFIG_DEFINITIONS} -DYY_NO_UNISTD_H" CACHE INTERNAL "libconfig definitions" )
endif()

include(CheckIncludeFile)
check_include_file(xlocale.h HAVE_LOCALE_H)

if(HAVE_LOCALE_H)
	set( LIBCONFIG_DEFINITIONS "${LIBCONFIG_DEFINITIONS} -DHAVE_XLOCALE_H" CACHE INTERNAL "libconfig definitions" )
endif()
