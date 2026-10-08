
#
# vendored source build (Windows) or system zlib (other platforms)
#
# Windows has no system zlib, so 1.3.2 is built from the vendored source as
# a static lib. This replaces the old prebuilt DLL/import-lib blobs and
# removes the runtime zlib.dll dependency. ZLIB_VERSION is read from the
# vendored header so configure reports the real version.
#
if( WIN32 )
message( STATUS "Detecting local ZLIB" )
find_path( ZLIB_LOCAL_INCLUDE_DIRS "zlib.h"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/include"
	NO_DEFAULT_PATH )
mark_as_advanced( ZLIB_LOCAL_LIBRARIES )
mark_as_advanced( ZLIB_LOCAL_INCLUDE_DIRS )

if( ZLIB_LOCAL_INCLUDE_DIRS )
	file( STRINGS "${ZLIB_LOCAL_INCLUDE_DIRS}/zlib.h" ZLIB_H REGEX "^#define[ \t]+ZLIB_VERSION[ \t]+\"[^\"]+\".*$" )
	string( REGEX REPLACE "^.*ZLIB_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" ZLIB_VERSION "${ZLIB_H}" )
	message( STATUS "Building vendored ZLIB from source (header version ${ZLIB_VERSION})" )
	set( ZLIB_LOCAL_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/src/adler32.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/compress.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/crc32.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/deflate.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/gzclose.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/gzlib.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/gzread.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/gzwrite.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/infback.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/inffast.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/inflate.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/inftrees.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/trees.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/uncompr.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/zutil.c"
		CACHE INTERNAL "vendored zlib sources" )
	add_library( zlib_local STATIC ${ZLIB_LOCAL_SOURCES} )
	target_include_directories( zlib_local PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src" )
	set( ZLIB_LOCAL_LIBRARIES zlib_local
		CACHE PATH "local zlib libraries" )
	set( HAVE_LOCAL_ZLIB ON
		CACHE BOOL "zlib is available as a local copy")
	mark_as_advanced( HAVE_LOCAL_ZLIB )
else()
	message( STATUS "Could NOT find local ZLIB (missing include/zlib.h)" )
	unset( HAVE_LOCAL_ZLIB CACHE )
endif()
message( STATUS "Detecting local ZLIB - done" )
endif( WIN32 )


#
# system
#
message( STATUS "Detecting system ZLIB" )
unset( ZLIB_LIBRARIES CACHE )
unset( ZLIB_INCLUDE_DIRS CACHE )
find_package( ZLIB )
set( ZLIB_SYSTEM_LIBRARIES ${ZLIB_LIBRARIES}
	CACHE PATH "system zlib libraries" )
set( ZLIB_SYSTEM_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS}
	CACHE PATH "system zlib include directories" )
mark_as_advanced( ZLIB_SYSTEM_LIBRARIES )
mark_as_advanced( ZLIB_SYSTEM_INCLUDE_DIRS )

if( ZLIB_SYSTEM_LIBRARIES AND ZLIB_SYSTEM_INCLUDE_DIRS )
	set( HAVE_SYSTEM_ZLIB ON
		CACHE BOOL "zlib is available on the system" )
	mark_as_advanced( HAVE_SYSTEM_ZLIB )
else()
	unset( HAVE_SYSTEM_ZLIB CACHE )
endif()
message( STATUS "Detecting system ZLIB - done" )


#
# configure
#
CONFIGURE_WITH_LOCAL_OR_SYSTEM( ZLIB )
