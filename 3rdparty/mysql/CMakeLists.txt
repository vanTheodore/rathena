#
# system
#
# The client is not vendored anymore: install MariaDB Connector/C (or a MySQL
# Connector/C >= 8.0) and find_package(MYSQL) locates it. See
# doc/MYSQL_MODERNIZATION.md for the supported versions, layouts and install
# commands.
#
message( STATUS "Detecting system MYSQL" )
unset( MYSQL_LIBRARIES CACHE )
unset( MYSQL_INCLUDE_DIRS CACHE )
# Drop the cache entries of the removed bundled client so an existing build tree
# does not keep pointing at 3rdparty/mysql/lib.
unset( HAVE_LOCAL_MYSQL CACHE )
unset( WITH_LOCAL_MYSQL CACHE )
unset( MYSQL_LOCAL_INCLUDE_DIRS CACHE )
unset( MYSQL_LOCAL_LIBRARIES CACHE )
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
# client version floor (see doc/MYSQL_MODERNIZATION.md)
#
# Login against current servers requires client-side caching_sha2_password
# support (usable since MariaDB Connector/C 3.0.8, or with a MySQL 8.0 client)
# and TLS 1.2; older clients also cannot talk to MySQL 8.4+/MariaDB 11 servers
# that no longer accept mysql_native_password. This is reported as a warning
# rather than an error so that an existing installation keeps building and the
# operator can read what to install.
if( MYSQL_VERSION_STRING )
	# MySQL client libraries carry the server version they were built for
	# (5.x, 6.1.x for the legacy Connector/C series, 8.x, 9.x) and need 8.0 for
	# caching_sha2_password; MariaDB Connector/C uses its own 2.x/3.x series and
	# needs 3.0.8. A version below 5 can only be MariaDB Connector/C or a
	# hint-only detection, so it is handled as MariaDB.
	if( MYSQL_CLIENT_KIND MATCHES "MariaDB" OR MYSQL_VERSION_STRING VERSION_LESS 5 )
		set( MYSQL_CLIENT_FLOOR "3.0.8" )
	else()
		set( MYSQL_CLIENT_FLOOR "8.0" )
	endif()
	if( MYSQL_VERSION_STRING VERSION_LESS MYSQL_CLIENT_FLOOR )
		message( WARNING
			"${MYSQL_CLIENT_KIND} ${MYSQL_VERSION_STRING} is older than the supported client floor "
			"(${MYSQL_CLIENT_FLOOR}). Logging in to MySQL 8.4+/MariaDB 11+ servers (caching_sha2_password) and TLS 1.2 "
			"will not work with this client. Install MariaDB Connector/C 3.4.x — see doc/MYSQL_MODERNIZATION.md." )
	endif()
	message( STATUS "Using MYSQL: ${MYSQL_LIBRARIES} (${MYSQL_CLIENT_KIND} ${MYSQL_VERSION_STRING})" )
else()
	message( STATUS "Using MYSQL: ${MYSQL_LIBRARIES} (client version unknown, floor not checked)" )
endif()
