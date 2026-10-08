
#
# vendored source build (Windows) or system PCRE2 (other platforms)
#
# PCRE1 is end-of-life; the implementation is PCRE2 10.49 (BSD-licensed),
# built from vendored 8-bit sources as a static lib. This replaces the old
# prebuilt pcre8 DLL/import-lib blobs and removes the runtime pcre8.dll
# dependency. Internal variable names keep the historic PCRE_* shape so
# consumers (src/map) need no churn; they refer to the regex engine, whose
# implementation is now PCRE2 (see 3rdparty/README.txt).
#
# No JIT: rAthena compiles tiny chat/script patterns and matches short
# subjects. pcre2_jit_compile.c is compiled (as upstream does) but JIT is
# never invoked, which also keeps the JIT-stack CVE class out of reach.
#
if( WIN32 )
message( STATUS "Detecting local PCRE" )
# Re-resolve on every configure: a cache inherited from the removed PCRE1
# tree would point at deleted paths (pcre8.lib / pcre/include).
unset( PCRE_LOCAL_INCLUDE_DIRS CACHE )
unset( PCRE_LOCAL_LIBRARIES CACHE )
find_path( PCRE_LOCAL_INCLUDE_DIRS "pcre2.h"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/include"
	NO_DEFAULT_PATH )
mark_as_advanced( PCRE_LOCAL_LIBRARIES )
mark_as_advanced( PCRE_LOCAL_INCLUDE_DIRS )

if( PCRE_LOCAL_INCLUDE_DIRS )
	file( STRINGS "${PCRE_LOCAL_INCLUDE_DIRS}/pcre2.h" PCRE2_H REGEX "^#define[ \t]+PCRE2_M[A-Z]+[ \t]+[0-9]+.*$" )
	string( REGEX REPLACE "^.*PCRE2_MAJOR[ \t]+([0-9]+).*$" "\\1" PCRE2_MAJOR "${PCRE2_H}" )
	string( REGEX REPLACE "^.*PCRE2_MINOR[ \t]+([0-9]+).*$" "\\1" PCRE2_MINOR "${PCRE2_H}" )
	message( STATUS "Building vendored PCRE2 from source (header version ${PCRE2_MAJOR}.${PCRE2_MINOR})" )
	set( PCRE_LOCAL_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_auto_possess.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_chartables.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_chkdint.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_compile.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_compile_cgroup.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_compile_class.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_config.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_context.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_convert.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_dfa_match.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_error.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_extuni.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_find_bracket.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_jit_compile.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_maketables.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_match.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_match_data.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_match_next.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_newline.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_ord2utf.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_pattern_info.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_script_run.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_serialize.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_string_utils.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_study.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_substitute.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_substring.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_tables.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_ucd.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_valid_utf.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/pcre2_xclass.c"
		CACHE INTERNAL "vendored pcre2 sources" )
	add_library( pcre2_local STATIC ${PCRE_LOCAL_SOURCES} )
	# Documented non-autotools path: no generated config.h; the 8-bit lib
	# needs no HAVE_* macros (standard headers are included directly).
	# PCRE2_EXPORT only exists via the generated config.h; for a static
	# build it must be empty (upstream sets it the same way).
	target_compile_definitions( pcre2_local PUBLIC
		PCRE2_CODE_UNIT_WIDTH=8
		SUPPORT_PCRE2_8=1
		PCRE2_STATIC )
	# No generated config.h (documented non-autotools path). Values below
	# are upstream's defaults from src/config.h.generic; only LINK_SIZE has
	# no in-code fallback (hard #error), the rest would also fail without
	# config.h (used raw in the default contexts).
	target_compile_definitions( pcre2_local PRIVATE
		PCRE2_EXPORT=
		LINK_SIZE=2
		HEAP_LIMIT=20000000
		MATCH_LIMIT=10000000
		MATCH_LIMIT_DEPTH=10000000
		MAX_VARLOOKBEHIND=255
		MAX_NAME_COUNT=10000
		MAX_NAME_SIZE=128
		NEWLINE_DEFAULT=2
		PARENS_NEST_LIMIT=250 )
	target_include_directories( pcre2_local PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src" )
	set( PCRE_LOCAL_LIBRARIES pcre2_local
		CACHE PATH "local pcre libraries" )
	set( HAVE_LOCAL_PCRE ON
		CACHE BOOL "pcre is available as a local copy" )
	mark_as_advanced( HAVE_LOCAL_PCRE )
else()
	message( STATUS "Could NOT find local PCRE (missing include/pcre2.h)" )
	unset( HAVE_LOCAL_PCRE CACHE )
endif()
message( STATUS "Detecting local PCRE - done" )
endif( WIN32 )


#
# system
#
message( STATUS "Detecting system PCRE2" )
unset( PCRE_LIBRARIES CACHE )
unset( PCRE_INCLUDE_DIRS CACHE )
# Re-resolve on every configure (stale PCRE1 caches point at deleted paths).
unset( PCRE_SYSTEM_LIBRARIES CACHE )
unset( PCRE_SYSTEM_INCLUDE_DIRS CACHE )
find_package( PCRE2 )
if( PCRE2_FOUND )
	# pcre2.h requires PCRE2_CODE_UNIT_WIDTH to be defined by whoever
	# includes it; rAthena uses the 8-bit library. The local (Windows) path
	# gets this from the pcre2_local target; the system path needs an
	# interface target to propagate it (plus include dirs and the library).
	add_library( pcre2_system INTERFACE )
	target_link_libraries( pcre2_system INTERFACE ${PCRE2_LIBRARIES} )
	target_include_directories( pcre2_system INTERFACE ${PCRE2_INCLUDE_DIRS} )
	target_compile_definitions( pcre2_system INTERFACE PCRE2_CODE_UNIT_WIDTH=8 )
	set( PCRE_SYSTEM_LIBRARIES pcre2_system
		CACHE PATH "system pcre libraries" )
	set( PCRE_SYSTEM_INCLUDE_DIRS ${PCRE2_INCLUDE_DIRS}
		CACHE PATH "system pcre include directories" )
endif()
mark_as_advanced( PCRE_SYSTEM_LIBRARIES )
mark_as_advanced( PCRE_SYSTEM_INCLUDE_DIRS )

if( PCRE_SYSTEM_LIBRARIES AND PCRE_SYSTEM_INCLUDE_DIRS )
	set( HAVE_SYSTEM_PCRE ON
		CACHE BOOL "pcre is available on the system" )
	mark_as_advanced( HAVE_SYSTEM_PCRE )
else()
	unset( HAVE_SYSTEM_PCRE CACHE )
endif()
message( STATUS "Detecting system PCRE2 - done" )


#
# configure
#
CONFIGURE_WITH_LOCAL_OR_SYSTEM( PCRE )
