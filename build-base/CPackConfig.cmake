# This file will be configured to contain variables for CPack. These variables
# should be set in the CMake list file of the project before CPack module is
# included. The list of available CPACK_xxx variables and their associated
# documentation may be obtained using
#  cpack --help-variable-list
#
# Some variables are common to all generators (e.g. CPACK_PACKAGE_NAME)
# and some are specific to a generator
# (e.g. CPACK_NSIS_EXTRA_INSTALL_COMMANDS). The generator specific variables
# usually begin with CPACK_<GENNAME>_xxxx.


set(CPACK_ARCHIVE_GID "-1")
set(CPACK_ARCHIVE_UID "-1")
set(CPACK_BINARY_7Z "OFF")
set(CPACK_BINARY_IFW "OFF")
set(CPACK_BINARY_INNOSETUP "OFF")
set(CPACK_BINARY_NSIS "ON")
set(CPACK_BINARY_NUGET "OFF")
set(CPACK_BINARY_WIX "OFF")
set(CPACK_BINARY_ZIP "OFF")
set(CPACK_BUILD_SOURCE_DIRS "C:/Users/LEGEN/Documents/rathena;C:/Users/LEGEN/Documents/rathena/build-base")
set(CPACK_CMAKE_GENERATOR "Ninja")
set(CPACK_COMPONENTS_ALL "")
set(CPACK_COMPONENT_UNSPECIFIED_HIDDEN "TRUE")
set(CPACK_COMPONENT_UNSPECIFIED_REQUIRED "TRUE")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_FILE "C:/Users/LEGEN/AppData/Local/Programs/Python/Python314/Lib/site-packages/cmake/data/share/cmake-4.4/Templates/CPack.GenericDescription.txt")
set(CPACK_DEFAULT_PACKAGE_DESCRIPTION_SUMMARY "rAthena built using CMake")
set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE "ON")
set(CPACK_GENERATOR "NSIS")
set(CPACK_INNOSETUP_ARCHITECTURE "x64")
set(CPACK_INSTALL_CMAKE_PROJECTS "C:/Users/LEGEN/Documents/rathena/build-base;rAthena;ALL;/")
set(CPACK_INSTALL_PREFIX "C:/Program Files (x86)/rAthena")
set(CPACK_MODULE_PATH "C:/Users/LEGEN/Documents/rathena/3rdparty/cmake")
set(CPACK_NSIS_DISPLAY_NAME "rAthena 0.1.1")
set(CPACK_NSIS_INSTALLER_ICON_CODE "")
set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_PACKAGE_NAME "rAthena 0.1.1")
set(CPACK_NSIS_UNINSTALL_NAME "Uninstall")
set(CPACK_OBJCOPY_EXECUTABLE "C:/msys64/ucrt64/bin/objcopy.exe")
set(CPACK_OBJDUMP_EXECUTABLE "C:/msys64/ucrt64/bin/objdump.exe")
set(CPACK_OUTPUT_CONFIG_FILE "C:/Users/LEGEN/Documents/rathena/build-base/CPackConfig.cmake")
set(CPACK_PACKAGE_DEFAULT_LOCATION "/")
set(CPACK_PACKAGE_DESCRIPTION_FILE "C:/Users/LEGEN/AppData/Local/Programs/Python/Python314/Lib/site-packages/cmake/data/share/cmake-4.4/Templates/CPack.GenericDescription.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MMORPG server package")
set(CPACK_PACKAGE_FILE_NAME "rAthena-0.1.1-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "rAthena 0.1.1")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "rAthena 0.1.1")
set(CPACK_PACKAGE_NAME "rAthena")
set(CPACK_PACKAGE_RELOCATABLE "true")
set(CPACK_PACKAGE_VENDOR "Humanity")
set(CPACK_PACKAGE_VERSION "0.1.1")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_READELF_EXECUTABLE "C:/msys64/ucrt64/bin/readelf.exe")
set(CPACK_RESOURCE_FILE_LICENSE "C:/Users/LEGEN/Documents/rathena/LICENSE")
set(CPACK_RESOURCE_FILE_README "C:/Users/LEGEN/AppData/Local/Programs/Python/Python314/Lib/site-packages/cmake/data/share/cmake-4.4/Templates/CPack.GenericDescription.txt")
set(CPACK_RESOURCE_FILE_WELCOME "C:/Users/LEGEN/AppData/Local/Programs/Python/Python314/Lib/site-packages/cmake/data/share/cmake-4.4/Templates/CPack.GenericWelcome.txt")
set(CPACK_SET_DESTDIR "OFF")
set(CPACK_SOURCE_7Z "ON")
set(CPACK_SOURCE_GENERATOR "7Z;ZIP")
set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "C:/Users/LEGEN/Documents/rathena/build-base/CPackSourceConfig.cmake")
set(CPACK_SOURCE_ZIP "ON")
set(CPACK_SYSTEM_NAME "win64")
set(CPACK_THREADS "1")
set(CPACK_TOPLEVEL_TAG "win64")
set(CPACK_WIX_SIZEOF_VOID_P "8")

if(NOT CPACK_PROPERTIES_FILE)
  set(CPACK_PROPERTIES_FILE "C:/Users/LEGEN/Documents/rathena/build-base/CPackProperties.cmake")
endif()

if(EXISTS ${CPACK_PROPERTIES_FILE})
  include(${CPACK_PROPERTIES_FILE})
endif()

# Configuration for component group "Runtime"
set(CPACK_COMPONENT_GROUP_RUNTIME_DISPLAY_NAME "Runtime")
set(CPACK_COMPONENT_GROUP_RUNTIME_DESCRIPTION "Runtime files")

# Configuration for component "Runtime_base"

SET(CPACK_COMPONENTS_ALL)
set(CPACK_COMPONENT_RUNTIME_BASE_DISPLAY_NAME "Base files")
set(CPACK_COMPONENT_RUNTIME_BASE_DESCRIPTION "configurations, dbs, npcs, docs, ...")
set(CPACK_COMPONENT_RUNTIME_BASE_GROUP Runtime)

# Configuration for component "Runtime_templates"

SET(CPACK_COMPONENTS_ALL)
set(CPACK_COMPONENT_RUNTIME_TEMPLATES_DISPLAY_NAME "Base templates")
set(CPACK_COMPONENT_RUNTIME_TEMPLATES_DESCRIPTION "conf/import and save (generated from conf/import-tmpl and save-tmpl)")
set(CPACK_COMPONENT_RUNTIME_TEMPLATES_GROUP Runtime)

# Configuration for component group "Development"
set(CPACK_COMPONENT_GROUP_DEVELOPMENT_DISPLAY_NAME "Development")
set(CPACK_COMPONENT_GROUP_DEVELOPMENT_DESCRIPTION "Development files")

# Configuration for component "Development_base"

SET(CPACK_COMPONENTS_ALL)
set(CPACK_COMPONENT_DEVELOPMENT_BASE_DISPLAY_NAME "Base files")
set(CPACK_COMPONENT_DEVELOPMENT_BASE_DESCRIPTION "projects, 3rdparty, sources, templates")
set(CPACK_COMPONENT_DEVELOPMENT_BASE_GROUP Development)

# Configuration for component "Runtime_loginserver"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_templates Unspecified)
set(CPACK_COMPONENT_RUNTIME_LOGINSERVER_DISPLAY_NAME "login-server")
set(CPACK_COMPONENT_RUNTIME_LOGINSERVER_DESCRIPTION "login-server")
set(CPACK_COMPONENT_RUNTIME_LOGINSERVER_GROUP Runtime)

# Configuration for component "Runtime_charserver"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_loginserver Runtime_templates Unspecified)
set(CPACK_COMPONENT_RUNTIME_CHARSERVER_DISPLAY_NAME "char-server")
set(CPACK_COMPONENT_RUNTIME_CHARSERVER_DESCRIPTION "char-server")
set(CPACK_COMPONENT_RUNTIME_CHARSERVER_GROUP Runtime)

# Configuration for component "Runtime_mapserver"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_charserver Runtime_loginserver Runtime_templates Unspecified)
set(CPACK_COMPONENT_RUNTIME_MAPSERVER_DISPLAY_NAME "map-server")
set(CPACK_COMPONENT_RUNTIME_MAPSERVER_DESCRIPTION "map-server")
set(CPACK_COMPONENT_RUNTIME_MAPSERVER_GROUP Runtime)

# Configuration for component "Runtime_webserver"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_charserver Runtime_loginserver Runtime_mapserver Runtime_templates Unspecified)
set(CPACK_COMPONENT_RUNTIME_WEBSERVER_DISPLAY_NAME "web-server")
set(CPACK_COMPONENT_RUNTIME_WEBSERVER_DESCRIPTION "web-server")
set(CPACK_COMPONENT_RUNTIME_WEBSERVER_GROUP Runtime)

# Configuration for component "Runtime_mapcache"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_charserver Runtime_loginserver Runtime_mapserver Runtime_templates Runtime_webserver Unspecified)
set(CPACK_COMPONENT_RUNTIME_MAPCACHE_DISPLAY_NAME "mapcache")
set(CPACK_COMPONENT_RUNTIME_MAPCACHE_DESCRIPTION "mapcache generator")
set(CPACK_COMPONENT_RUNTIME_MAPCACHE_GROUP Runtime)

# Configuration for component "Runtime_csv2yaml"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_charserver Runtime_loginserver Runtime_mapcache Runtime_mapserver Runtime_templates Runtime_webserver Unspecified)
set(CPACK_COMPONENT_RUNTIME_CSV2YAML_DISPLAY_NAME "csv2yaml")
set(CPACK_COMPONENT_RUNTIME_CSV2YAML_DESCRIPTION "yaml converter")
set(CPACK_COMPONENT_RUNTIME_CSV2YAML_GROUP Runtime)

# Configuration for component "Runtime_yaml2sql"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_charserver Runtime_csv2yaml Runtime_loginserver Runtime_mapcache Runtime_mapserver Runtime_templates Runtime_webserver Unspecified)
set(CPACK_COMPONENT_RUNTIME_YAML2SQL_DISPLAY_NAME "yaml2sql")
set(CPACK_COMPONENT_RUNTIME_YAML2SQL_DESCRIPTION "sql converter")
set(CPACK_COMPONENT_RUNTIME_YAML2SQL_GROUP Runtime)

# Configuration for component "Runtime_yamlupgrade"

SET(CPACK_COMPONENTS_ALL Runtime_base Runtime_charserver Runtime_csv2yaml Runtime_loginserver Runtime_mapcache Runtime_mapserver Runtime_templates Runtime_webserver Runtime_yaml2sql Unspecified)
set(CPACK_COMPONENT_RUNTIME_YAMLUPGRADE_DISPLAY_NAME "yamlupgrade")
set(CPACK_COMPONENT_RUNTIME_YAMLUPGRADE_DESCRIPTION "yaml upgrader")
set(CPACK_COMPONENT_RUNTIME_YAMLUPGRADE_GROUP Runtime)
