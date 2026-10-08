# - Find PCRE2 (8-bit library)
# Find the native PCRE2 includes and library
#
#  PCRE2_INCLUDE_DIRS - where to find pcre2.h
#  PCRE2_LIBRARIES    - List of libraries when using pcre2.
#  PCRE2_FOUND        - True if pcre2 found.
#
# Note: rAthena keeps the historic PCRE_LIBRARIES/PCRE_INCLUDE_DIRS
# variable names for consumers; this module only locates the artifacts.

find_path( PCRE2_INCLUDE_DIR pcre2.h )
set( PCRE2_NAMES pcre2-8 )
find_library( PCRE2_LIBRARY NAMES ${PCRE2_NAMES} )
mark_as_advanced( PCRE2_LIBRARY PCRE2_INCLUDE_DIR )

if( PCRE2_INCLUDE_DIR AND EXISTS "${PCRE2_INCLUDE_DIR}/pcre2.h" )
	file( STRINGS "${PCRE2_INCLUDE_DIR}/pcre2.h" PCRE2_H REGEX "^#define[ \t]+PCRE2_M[A-Z]+[ \t]+[0-9]+.*$" )
	string( REGEX REPLACE "^.*PCRE2_MAJOR[ \t]+([0-9]+).*$" "\\1" PCRE2_MAJOR "${PCRE2_H}" )
	string( REGEX REPLACE "^.*PCRE2_MINOR[ \t]+([0-9]+).*$" "\\1" PCRE2_MINOR "${PCRE2_H}" )

	set( PCRE2_VERSION_STRING "${PCRE2_MAJOR}.${PCRE2_MINOR}" )
	set( PCRE2_VERSION_MAJOR "${PCRE2_MAJOR}" )
	set( PCRE2_VERSION_MINOR "${PCRE2_MINOR}" )
endif()

# handle the QUIETLY and REQUIRED arguments and set PCRE2_FOUND to TRUE if
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( PCRE2
	REQUIRED_VARS PCRE2_LIBRARY PCRE2_INCLUDE_DIR
	VERSION_VAR PCRE2_VERSION_STRING )

if( PCRE2_FOUND )
	set( PCRE2_LIBRARIES ${PCRE2_LIBRARY} )
	set( PCRE2_INCLUDE_DIRS ${PCRE2_INCLUDE_DIR} )
endif()
