#
# local copy
#
if( WIN32 )
message( STATUS "Detecting local MYSQL" )
find_path( MYSQL_LOCAL_INCLUDE_DIRS "mysql.h"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/include"
	NO_DEFAULT_PATH )
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
find_library( MYSQL_LOCAL_LIBRARIES
	NAMES libmysql
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib/Win32"
	NO_DEFAULT_PATH )
else()
find_library( MYSQL_LOCAL_LIBRARIES
	NAMES libmysql
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib/x64"
	NO_DEFAULT_PATH )
endif()
mark_as_advanced( MYSQL_LOCAL_LIBRARIES )
mark_as_advanced( MYSQL_LOCAL_INCLUDE_DIRS )

if( MYSQL_LOCAL_LIBRARIES AND MYSQL_LOCAL_INCLUDE_DIRS )
	# The bundled copy is MySQL Connector/C; report the client library version
	# (LIBMYSQL_VERSION), not the server string the headers were generated for.
	set( MYSQL_LOCAL_CLIENT_KIND "MySQL Connector/C (bundled)" )
	if( EXISTS "${MYSQL_LOCAL_INCLUDE_DIRS}/mysql_version.h" )
		file( STRINGS "${MYSQL_LOCAL_INCLUDE_DIRS}/mysql_version.h" LIBMYSQL_VERSION_H REGEX "^#define[ \t]+LIBMYSQL_VERSION[ \t]+\"[^\"]+\".*$" )
		if( LIBMYSQL_VERSION_H )
			string( REGEX REPLACE "^.*LIBMYSQL_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_LOCAL_VERSION_STRING "${LIBMYSQL_VERSION_H}" )
		else()
			file( STRINGS "${MYSQL_LOCAL_INCLUDE_DIRS}/mysql_version.h" MYSQL_SERVER_VERSION_H REGEX "^#define MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
			string( REGEX REPLACE "^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_LOCAL_VERSION_STRING "${MYSQL_SERVER_VERSION_H}" )
		endif()
	endif()
	if( MYSQL_LOCAL_VERSION_STRING )
		message( STATUS "Found MYSQL: ${MYSQL_LOCAL_LIBRARIES} (found version ${MYSQL_LOCAL_VERSION_STRING})" )
	else()
		message( STATUS "Found MYSQL: ${MYSQL_LOCAL_LIBRARIES}" )
	endif()
	set( HAVE_LOCAL_MYSQL ON
		CACHE BOOL "mysql client is available as a local copy")
	mark_as_advanced( HAVE_LOCAL_MYSQL )
else()
	foreach( _VAR MYSQL_LOCAL_LIBRARIES MYSQL_LOCAL_INCLUDE_DIRS )
		if( NOT "${_VAR}" )
			set( MISSING_VARS ${MISSING_VARS} ${_VAR} )
		endif()
	endforeach()
	message( STATUS "Could NOT find MYSQL (missing: ${MISSING_VARS})" )
	unset( HAVE_LOCAL_MYSQL CACHE )
endif()
message( STATUS "Detecting local MYSQL - done" )
endif( WIN32 )


#
# system
#
message( STATUS "Detecting system MYSQL" )
unset( MYSQL_LIBRARIES CACHE )
unset( MYSQL_INCLUDE_DIRS CACHE )
find_package( MYSQL )
set( MYSQL_SYSTEM_LIBRARIES "${MYSQL_LIBRARIES}"
	CACHE PATH "system mysql libraries" )
set( MYSQL_SYSTEM_INCLUDE_DIRS "${MYSQL_INCLUDE_DIRS}"
	CACHE PATH "system mysql include directories" )
mark_as_advanced( MYSQL_SYSTEM_LIBRARIES )
mark_as_advanced( MYSQL_SYSTEM_INCLUDE_DIRS )

if( MYSQL_SYSTEM_LIBRARIES AND MYSQL_SYSTEM_INCLUDE_DIRS )
	set( HAVE_SYSTEM_MYSQL ON
		CACHE BOOL "mysql client is available on the system" )
	mark_as_advanced( HAVE_SYSTEM_MYSQL )
else()
	unset( HAVE_SYSTEM_MYSQL CACHE )
endif()
message( STATUS "Detecting system MYSQL - done" )


#
# configure
#
CONFIGURE_WITH_LOCAL_OR_SYSTEM( MYSQL )


#
# client version floor (see doc/MYSQL_MODERNIZATION.md, stage S2)
#
# Login against current servers requires client-side caching_sha2_password
# support (usable since MariaDB Connector/C 3.0.8, or a MySQL 8.0 client) and
# TLS 1.2. Old clients also cannot talk to MySQL 8.4+/MariaDB 11 servers that
# no longer accept mysql_native_password. This is a warning rather than an
# error so existing setups keep building while the legacy bundled client is
# replaced (stage S3).
if( HAVE_LOCAL_MYSQL )
	set( MYSQL_SELECTED_CLIENT_KIND "${MYSQL_LOCAL_CLIENT_KIND}" )
	set( MYSQL_SELECTED_CLIENT_VERSION "${MYSQL_LOCAL_VERSION_STRING}" )
elseif( HAVE_SYSTEM_MYSQL )
	set( MYSQL_SELECTED_CLIENT_KIND "${MYSQL_CLIENT_KIND}" )
	set( MYSQL_SELECTED_CLIENT_VERSION "${MYSQL_VERSION_STRING}" )
endif()

if( MYSQL_SELECTED_CLIENT_VERSION )
	# MySQL client libraries carry the server version they were built for
	# (5.x, 6.1.x for the legacy Connector/C series, 8.x, 9.x) and need 8.0 for
	# caching_sha2_password; MariaDB Connector/C uses its own 2.x/3.x series
	# and needs 3.0.8. A version below 5 can only be MariaDB Connector/C or an
	# unknown-source hint, so it is handled as MariaDB.
	if( MYSQL_SELECTED_CLIENT_KIND MATCHES "MariaDB" OR MYSQL_SELECTED_CLIENT_VERSION VERSION_LESS 5 )
		set( MYSQL_CLIENT_FLOOR "3.0.8" )
	else()
		set( MYSQL_CLIENT_FLOOR "8.0" )
	endif()
	if( MYSQL_SELECTED_CLIENT_VERSION VERSION_LESS MYSQL_CLIENT_FLOOR )
		message( WARNING
			"${MYSQL_SELECTED_CLIENT_KIND} ${MYSQL_SELECTED_CLIENT_VERSION} is older than the supported client floor "
			"(${MYSQL_CLIENT_FLOOR}). Logging in to MySQL 8.4+/MariaDB 11+ servers (caching_sha2_password) and TLS 1.2 "
			"will not work with this client. Install MariaDB Connector/C 3.4.x and rebuild." )
	endif()
	message( STATUS "Using MYSQL: ${MYSQL_LIBRARIES} (${MYSQL_SELECTED_CLIENT_KIND} ${MYSQL_SELECTED_CLIENT_VERSION})" )
else()
	message( STATUS "Using MYSQL: ${MYSQL_LIBRARIES} (client version unknown, floor not checked)" )
endif()
