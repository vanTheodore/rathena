# - Find the MySQL/MariaDB client C library
# Finds the native MySQL (libmysqlclient) or MariaDB Connector/C (libmariadb)
# includes and library. Both implement the same client API; MariaDB
# Connector/C is what modern distributions package.
#
#  MYSQL_INCLUDE_DIRS - where to find mysql.h, etc.
#  MYSQL_LIBRARIES   - the client library (libmysqlclient or libmariadb).
#  MYSQL_FOUND       - True if a client library is found.
#

find_path( MYSQL_INCLUDE_DIRS "mysql.h"
	PATHS
		"/usr/include/mysql"
		"/usr/include/mariadb" # Debian/Ubuntu libmariadb-dev
		"/usr/local/include/mysql"
		"/usr/local/include/mariadb"
		"/usr/mysql/include/mysql"
		"/opt/homebrew/include/mysql" # for Apple Silicon (M1, M2)
		"/opt/homebrew/include/mariadb" # MariaDB Connector/C
		"/usr/local/include/mysql" # for Intel-based Macs
		"/usr/local/include/mariadb"
		"$ENV{PROGRAMFILES}/MySQL/*/include"
		"$ENV{SYSTEMDRIVE}/MySQL/*/include"
		"$ENV{PROGRAMFILES}/MariaDB/*/include"
		"$ENV{SYSTEMDRIVE}/MariaDB/*/include" )

find_library( MYSQL_LIBRARIES
	NAMES "libmysql" "mysqlclient" "mysqlclient_r"
		"libmariadb" "mariadb" "mariadbclient" # MariaDB Connector/C
	PATHS
		"/usr/lib/mysql"
		"/usr/local/lib/mysql"
		"/usr/mysql/lib/mysql"
		"/opt/homebrew/opt/mysql/lib" # for Apple Silicon (M1, M2)
		"/opt/homebrew/opt/mariadb/lib"
		"/usr/local/opt/mysql/lib" # for Intel-based Macs
		"/usr/local/opt/mariadb/lib"
		"$ENV{PROGRAMFILES}/MySQL/*/lib"
		"$ENV{SYSTEMDRIVE}/MySQL/*/lib"
		"$ENV{PROGRAMFILES}/MariaDB/*/lib"
		"$ENV{SYSTEMDRIVE}/MariaDB/*/lib" )
mark_as_advanced( MYSQL_LIBRARIES  MYSQL_INCLUDE_DIRS )

if( MYSQL_INCLUDE_DIRS )
	# MariaDB Connector/C reports its own connector version (e.g. 3.4.11);
	# prefer it over the server-version string baked into mysql_version.h.
	if( EXISTS "${MYSQL_INCLUDE_DIRS}/mariadb_version.h" )
		file( STRINGS "${MYSQL_INCLUDE_DIRS}/mariadb_version.h" MARIADB_VERSION_H REGEX "^#define[ \t]+MARIADB_PACKAGE_VERSION[ \t]+\"[^\"]+\".*$" )
		if( MARIADB_VERSION_H )
			string( REGEX REPLACE "^.*MARIADB_PACKAGE_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING "${MARIADB_VERSION_H}" )
		endif()
	endif()
	if( NOT MYSQL_VERSION_STRING AND EXISTS "${MYSQL_INCLUDE_DIRS}/mysql_version.h" )
		file( STRINGS "${MYSQL_INCLUDE_DIRS}/mysql_version.h" MYSQL_VERSION_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
		string( REGEX REPLACE "^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING "${MYSQL_VERSION_H}" )
	endif()
endif()

# handle the QUIETLY and REQUIRED arguments and set MYSQL_FOUND to TRUE if 
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( MYSQL
	REQUIRED_VARS MYSQL_LIBRARIES MYSQL_INCLUDE_DIRS
	VERSION_VAR MYSQL_VERSION_STRING )
