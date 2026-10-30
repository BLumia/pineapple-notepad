# FindLexilla.cmake
#
# This module tries to find Lexilla, and if not found, fetches and builds it using FetchContent.
# It also assumes Scintilla is available (either found or fetched).
#
# Once done this will define
#  Lexilla_FOUND        - True if Lexilla was found
#  Lexilla_LIBRARIES    - List of libraries to link against (e.g., lexilla)
#  Lexilla_INCLUDE_DIRS - The Lexilla include directories

# Check if Lexilla is already found or if we are in a NO_MODULE situation
if(Lexilla_FOUND)
    return()
endif()

cmake_minimum_required(VERSION 3.16) # FetchContent requires CMake 3.14+, using 3.16 for consistency

include(GNUInstallDirs)
include(FetchContent)
include(PnDependencies)

# All version/url/hash metadata lives in dependencies.json; offline
# archives under offline/ are picked up automatically when present.
pn_declare_dependency(lexilla)

FetchContent_MakeAvailable(lexilla)

# Lexilla needs Scintilla headers, so ensure Scintilla is found/fetched
find_package(Scintilla REQUIRED)

add_library(lexilla SHARED)

# Set C++ standard for this target
target_compile_features(lexilla PUBLIC cxx_std_17)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    # main library binding
    "${lexilla_SOURCE_DIR}/include/Lexilla.h"
    "${lexilla_SOURCE_DIR}/src/Lexilla.cxx"
    # lexlib
    "${lexilla_SOURCE_DIR}/lexlib/*.h"
    "${lexilla_SOURCE_DIR}/lexlib/*.cxx"
    # lexers
    "${lexilla_SOURCE_DIR}/lexers/Lex*.cxx"
)

target_sources(lexilla
PRIVATE
    ${SRCS}
)

target_include_directories(lexilla
PUBLIC
    "${lexilla_SOURCE_DIR}/include"
    "${lexilla_SOURCE_DIR}/lexlib"
)

target_link_libraries(lexilla PUBLIC Scintilla::Scintilla)

# Set variables for find_package
set(Lexilla_FOUND TRUE)
set(Lexilla_INCLUDE_DIRS
    "${lexilla_SOURCE_DIR}/include"
    "${lexilla_SOURCE_DIR}/lexlib"
)

# Create an ALIAS target for modern CMake consumption
if(NOT TARGET Lexilla::Lexilla)
    add_library(Lexilla::Lexilla ALIAS lexilla)
endif()

# Set variables for find_package (for backward compatibility)
set(Lexilla_LIBRARIES Lexilla::Lexilla) # Point to the ALIAS target

# Mark as advanced so it doesn't show up in GUI by default
mark_as_advanced(Lexilla_FOUND Lexilla_LIBRARIES Lexilla_INCLUDE_DIRS)

install(TARGETS lexilla
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
