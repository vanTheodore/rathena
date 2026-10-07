# Install script for directory: C:/Users/LEGEN/Documents/rathena

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/LEGEN/Documents/rathena/build-clang/install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/msys64/ucrt64/bin/objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}" TYPE FILE FILES
    "C:/Users/LEGEN/Documents/rathena/athena-start"
    "C:/Users/LEGEN/Documents/rathena/LICENSE"
    "C:/Users/LEGEN/Documents/rathena/README.md"
    "C:/Users/LEGEN/Documents/rathena/dbghelp.dll"
    "C:/Users/LEGEN/Documents/rathena/libmysql.dll"
    "C:/Users/LEGEN/Documents/rathena/pcre3.dll"
    "C:/Users/LEGEN/Documents/rathena/zlib1.dll"
    "C:/Users/LEGEN/Documents/rathena/tools/charserv.bat"
    "C:/Users/LEGEN/Documents/rathena/tools/logserv.bat"
    "C:/Users/LEGEN/Documents/rathena/tools/mapserv.bat"
    "C:/Users/LEGEN/Documents/rathena/tools/runserver.bat"
    "C:/Users/LEGEN/Documents/rathena/tools/serv.bat"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/conf" TYPE DIRECTORY FILES "C:/Users/LEGEN/Documents/rathena/conf/" REGEX "/[\\\\.]svn$" EXCLUDE REGEX "/conf\\/import\\-tmpl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/db" TYPE DIRECTORY FILES "C:/Users/LEGEN/Documents/rathena/db/" REGEX "/[\\\\.]svn$" EXCLUDE REGEX "/conf\\/import\\-tmpl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/doc" TYPE DIRECTORY FILES "C:/Users/LEGEN/Documents/rathena/doc/" REGEX "/[\\\\.]svn$" EXCLUDE REGEX "/conf\\/import\\-tmpl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(MAKE_DIRECTORY "${ENV}${CMAKE_INSTALL_PREFIX}/log")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/npc" TYPE DIRECTORY FILES "C:/Users/LEGEN/Documents/rathena/npc/" REGEX "/[\\\\.]svn$" EXCLUDE REGEX "/conf\\/import\\-tmpl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/sql-files" TYPE DIRECTORY FILES "C:/Users/LEGEN/Documents/rathena/sql-files/" REGEX "/[\\\\.]svn$" EXCLUDE REGEX "/conf\\/import\\-tmpl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_base" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/tools" TYPE DIRECTORY FILES "C:/Users/LEGEN/Documents/rathena/tools/" REGEX "/[\\\\.]svn$" EXCLUDE REGEX "/conf\\/import\\-tmpl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Runtime_templates" OR NOT CMAKE_INSTALL_COMPONENT)
  include("C:/Users/LEGEN/Documents/rathena/build-clang/InstallTemplates.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/Users/LEGEN/Documents/rathena/build-clang/3rdparty/cmake_install.cmake")
  include("C:/Users/LEGEN/Documents/rathena/build-clang/src/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/LEGEN/Documents/rathena/build-clang/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/LEGEN/Documents/rathena/build-clang/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
