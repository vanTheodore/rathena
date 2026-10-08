# - Find the MySQL/MariaDB client C library
# Finds the native MySQL (libmysqlclient) or MariaDB Connector/C (libmariadb)
# includes and library. Both implement the same client API; MariaDB
# Connector/C is what modern distributions package.
#
#  MYSQL_INCLUDE_DIRS   - where to find mysql.h, etc.
#  MYSQL_LIBRARIES      - the client library (libmysqlclient or libmariadb).
#  MYSQL_CLIENT_KIND    - "MariaDB Connector/C" or "MySQL Connector/C".
#  MYSQL_VERSION_STRING - version of the client library (e.g. 3.4.11, 8.4.0).
#  MYSQL_FOUND          - True if a client library is found.
#
# pkg-config is used as a hint only, and only when the tool is installed:
# distribution packages ship libmariadb.pc/mysqlclient.pc with the include
# dir, library dir and connector version. MSVC builds normally have no
# pkg-config and fall back to the manual search below.

find_package( PkgConfig QUIET )
if( PKG_CONFIG_FOUND )
	pkg_check_modules( MYSQL_PC_MARIADB QUIET libmariadb )
	pkg_check_modules( MYSQL_PC_MYSQL QUIET mysqlclient )
	if( MYSQL_PC_MARIADB_FOUND OR MYSQL_PC_MYSQL_FOUND )
		message( STATUS "MYSQL: considering pkg-config hints (libmariadb ${MYSQL_PC_MARIADB_VERSION} mysqlclient ${MYSQL_PC_MYSQL_VERSION})" )
	endif()
endif()

# Prefixed installs keep the headers in <prefix>/include/mariadb: MSYS2/MinGW
# toolchains install the connector inside their own prefix, and any install can
# be pointed at with -DCMAKE_PREFIX_PATH=<MariaDB Connector C dir>.
set( MYSQL_PREFIX_INCLUDE_HINTS )
set( MYSQL_PREFIX_LIBRARY_HINTS )
set( _mysql_prefixes ${CMAKE_PREFIX_PATH} ${CMAKE_SYSTEM_PREFIX_PATH} )
if( CMAKE_CXX_COMPILER )
	get_filename_component( _mysql_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY )
	get_filename_component( _mysql_toolchain_prefix "${_mysql_compiler_dir}" DIRECTORY )
	list( APPEND _mysql_prefixes "${_mysql_toolchain_prefix}" )
endif()
foreach( _mysql_prefix ${_mysql_prefixes} )
	if( _mysql_prefix )
		list( APPEND MYSQL_PREFIX_INCLUDE_HINTS "${_mysql_prefix}/include/mariadb" )
		list( APPEND MYSQL_PREFIX_LIBRARY_HINTS "${_mysql_prefix}/lib" )
	endif()
endforeach()

find_path( MYSQL_INCLUDE_DIRS "mysql.h"
	HINTS
		${MYSQL_PC_MARIADB_INCLUDE_DIRS}
		${MYSQL_PC_MYSQL_INCLUDE_DIRS}
		${MYSQL_PREFIX_INCLUDE_HINTS}
	PATHS
		"/usr/include/mysql"
		"/usr/include/mariadb" # Debian/Ubuntu libmariadb-dev
		"/usr/local/include/mysql"
		"/usr/local/include/mariadb"
		"/usr/mysql/include/mysql"
		"/opt/homebrew/include/mysql" # for Apple Silicon (M1, M2)
		"/opt/homebrew/include/mariadb" # MariaDB Connector/C
		"/opt/homebrew/opt/mariadb-connector-c/include/mariadb" # brew formula
		"/usr/local/include/mysql" # for Intel-based Macs
		"/usr/local/include/mariadb"
		"/usr/local/opt/mariadb-connector-c/include/mariadb" # brew formula (Intel)
		"$ENV{PROGRAMFILES}/MySQL/*/include"
		"$ENV{PROGRAMW6432}/MySQL/*/include"
		"$ENV{SYSTEMDRIVE}/MySQL/*/include"
		"$ENV{PROGRAMFILES}/MariaDB/*/include"
		"$ENV{PROGRAMFILES}/MariaDB/*/include/mariadb" # MariaDB Connector C MSI
		"$ENV{PROGRAMW6432}/MariaDB/*/include"
		"$ENV{PROGRAMW6432}/MariaDB/*/include/mariadb" # 64-bit MSI, 32-bit CMake
		"$ENV{SYSTEMDRIVE}/MariaDB/*/include"
		"$ENV{SYSTEMDRIVE}/MariaDB/*/include/mariadb" )

find_library( MYSQL_LIBRARIES
	NAMES "libmysql" "mysqlclient" "mysqlclient_r"
		"libmariadb" "mariadb" "mariadbclient" # MariaDB Connector/C
	HINTS
		${MYSQL_PC_MARIADB_LIBRARY_DIRS}
		${MYSQL_PC_MYSQL_LIBRARY_DIRS}
		${MYSQL_PREFIX_LIBRARY_HINTS}
	PATHS
		"/usr/lib/mysql"
		"/usr/local/lib/mysql"
		"/usr/mysql/lib/mysql"
		"/opt/homebrew/opt/mysql/lib" # for Apple Silicon (M1, M2)
		"/opt/homebrew/opt/mariadb/lib"
		"/opt/homebrew/opt/mariadb-connector-c/lib" # brew formula
		"/usr/local/opt/mysql/lib" # for Intel-based Macs
		"/usr/local/opt/mariadb/lib"
		"/usr/local/opt/mariadb-connector-c/lib" # brew formula (Intel)
		"$ENV{PROGRAMFILES}/MySQL/*/lib"
		"$ENV{PROGRAMW6432}/MySQL/*/lib"
		"$ENV{SYSTEMDRIVE}/MySQL/*/lib"
		"$ENV{PROGRAMFILES}/MariaDB/*/lib"
		"$ENV{PROGRAMW6432}/MariaDB/*/lib"
		"$ENV{SYSTEMDRIVE}/MariaDB/*/lib" )
mark_as_advanced( MYSQL_LIBRARIES  MYSQL_INCLUDE_DIRS )

if( MYSQL_INCLUDE_DIRS )
	# MariaDB Connector/C reports its own connector version (e.g. 3.4.11);
	# prefer it over the server-version string baked into mysql_version.h.
	if( EXISTS "${MYSQL_INCLUDE_DIRS}/mariadb_version.h" )
		set( MYSQL_CLIENT_KIND "MariaDB Connector/C" )
		file( STRINGS "${MYSQL_INCLUDE_DIRS}/mariadb_version.h" MARIADB_PACKAGE_VERSION_H REGEX "^#define[ \t]+MARIADB_PACKAGE_VERSION[ \t]+\"[^\"]+\".*$" )
		if( MARIADB_PACKAGE_VERSION_H )
			string( REGEX REPLACE "^.*MARIADB_PACKAGE_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING "${MARIADB_PACKAGE_VERSION_H}" )
		endif()
	elseif( EXISTS "${MYSQL_INCLUDE_DIRS}/mysql_version.h" )
		set( MYSQL_CLIENT_KIND "MySQL Connector/C" )
		# LIBMYSQL_VERSION is the client library's own version (8.x, or 6.1.x
		# for the legacy Connector/C series). MYSQL_SERVER_VERSION is the server
		# string the headers were generated for, kept only as a fallback.
		file( STRINGS "${MYSQL_INCLUDE_DIRS}/mysql_version.h" LIBMYSQL_VERSION_H REGEX "^#define[ \t]+LIBMYSQL_VERSION[ \t]+\"[^\"]+\".*$" )
		if( LIBMYSQL_VERSION_H )
			string( REGEX REPLACE "^.*LIBMYSQL_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING "${LIBMYSQL_VERSION_H}" )
		else()
			file( STRINGS "${MYSQL_INCLUDE_DIRS}/mysql_version.h" MYSQL_SERVER_VERSION_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
			string( REGEX REPLACE "^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING "${MYSQL_SERVER_VERSION_H}" )
		endif()
	endif()
	# Last resort: the version pkg-config reported for the module found. The
	# kind matters as much as the version (the floor differs per connector).
	if( NOT MYSQL_VERSION_STRING )
		if( MYSQL_PC_MARIADB_VERSION )
			set( MYSQL_VERSION_STRING "${MYSQL_PC_MARIADB_VERSION}" )
			set( MYSQL_CLIENT_KIND "MariaDB Connector/C" )
		elseif( MYSQL_PC_MYSQL_VERSION )
			set( MYSQL_VERSION_STRING "${MYSQL_PC_MYSQL_VERSION}" )
			set( MYSQL_CLIENT_KIND "MySQL Connector/C" )
		endif()
	endif()
endif()

# handle the QUIETLY and REQUIRED arguments and set MYSQL_FOUND to TRUE if 
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( MYSQL
	REQUIRED_VARS MYSQL_LIBRARIES MYSQL_INCLUDE_DIRS
	VERSION_VAR MYSQL_VERSION_STRING )
