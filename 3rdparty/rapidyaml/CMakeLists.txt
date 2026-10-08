cmake_minimum_required(VERSION 3.13)

# Vendored rapidyaml v0.16.0 (pinned stable tag, MIT).
# Upstream 0.16 removed the c4core git submodule: only the required c4core
# sources are vendored under ext/c4core.src/ (see ext/c4core.src.txt).
# This file deliberately does NOT use upstream's c4Project macro machinery;
# it builds a plain static lib like the other vendored dependencies.
SET (this_target ryml)
PROJECT(${this_target})

find_path( RYML_INCLUDE_DIRS "ryml.hpp"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src"
	NO_DEFAULT_PATH )
find_path( RYML_SOURCE_DIR "c4/yml/tree.cpp"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src"
	NO_DEFAULT_PATH )
find_path( C4CORE_SOURCE_DIR "c4/substr.hpp"
	PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ext/c4core.src"
	NO_DEFAULT_PATH )
mark_as_advanced( RYML_INCLUDE_DIRS )
mark_as_advanced( RYML_SOURCE_DIR )
mark_as_advanced( C4CORE_SOURCE_DIR )

set( RYML_SOURCES
	"${RYML_SOURCE_DIR}/c4/yml/common.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/emit_buf.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/emit_file.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/node_type.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/parse.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/reference_resolver.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/scalar_style.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/tag.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/tree.cpp"
	"${RYML_SOURCE_DIR}/c4/yml/version.cpp"
	CACHE INTERNAL "ryml sources" )
# Mirror of ext/c4core.src.txt (required c4core sources, no submodule).
set( C4CORE_SOURCES
	"${C4CORE_SOURCE_DIR}/c4/base64.cpp"
	"${C4CORE_SOURCE_DIR}/c4/error.cpp"
	"${C4CORE_SOURCE_DIR}/c4/format.cpp"
	"${C4CORE_SOURCE_DIR}/c4/language.cpp"
	"${C4CORE_SOURCE_DIR}/c4/memory_util.cpp"
	"${C4CORE_SOURCE_DIR}/c4/utf.cpp"
	"${C4CORE_SOURCE_DIR}/c4/version.cpp"
	CACHE INTERNAL "c4core sources" )

ADD_LIBRARY(${this_target} STATIC ${RYML_SOURCES} ${C4CORE_SOURCES} )
target_include_directories(${this_target} SYSTEM PUBLIC
	${RYML_INCLUDE_DIRS}
	${C4CORE_SOURCE_DIR} )
# 0.16 deprecates the NodeRef >>= / <<= / |= / = operators rAthena uses
# (database.cpp). Define the documented legacy macro to keep them without
# deprecation warnings; migrating to .deserialize()/.set_*() is follow-up
# work (see doc/MODERNIZATION.md), not part of this dependency update.
target_compile_definitions(${this_target} PUBLIC RYML_WITH_LEGACY_OPERATORS)
