# FindScintilla.cmake
#
# This module tries to find Scintilla, and if not found, fetches and builds it using FetchContent.
#
# Once done this will define
#  Scintilla_FOUND        - True if Scintilla was found
#  Scintilla_LIBRARIES    - List of libraries to link against (e.g., scintilla-qt)
#  Scintilla_INCLUDE_DIRS - The Scintilla include directories

# Check if Scintilla is already found or if we are in a NO_MODULE situation
if(Scintilla_FOUND)
    return()
endif()

cmake_minimum_required(VERSION 3.16) # FetchContent requires CMake 3.14+, using 3.16 for consistency

include(GNUInstallDirs)
include(FetchContent)
include(PnDependencies)

# All version/url/hash metadata lives in dependencies.json; offline
# archives under offline/ are picked up automatically when present.
# The bug2500-thin-line patch originates from upstream scintilla:
# https://sourceforge.net/p/scintilla/code/ci/0b64765c461d71d53139c2c93573b71132bf6aef/
pn_declare_dependency(scintilla)

FetchContent_MakeAvailable(scintilla)

# Ensure Qt6 and Python3 are found for Scintilla's build
find_package(Qt6 REQUIRED
    COMPONENTS Widgets Core5Compat
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Run WidgetGen.py to generate ScintillaEdit.h and ScintillaEdit.cpp
# This needs to be done before compiling the Qt wrapper
add_custom_command(
    OUTPUT "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.h"
           "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.cpp"
    COMMAND ${CMAKE_COMMAND} -E chdir "${scintilla_SOURCE_DIR}/qt/ScintillaEdit"
            ${Python3_EXECUTABLE} WidgetGen.py
    DEPENDS "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/WidgetGen.py"
            "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.h.template"
            "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.cpp.template"
    COMMENT "Generating ScintillaEdit.h and ScintillaEdit.cpp"
)

add_custom_target(scintilla-widgetgen
    DEPENDS
        "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.h"
        "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.cpp"
)

add_library(scintilla-qt SHARED)

set_target_properties(scintilla-qt PROPERTIES AUTOMOC ON)

# Set C++ standard for this target
target_compile_features(scintilla-qt PUBLIC cxx_std_17)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "${scintilla_SOURCE_DIR}/include/*.h"
    "${scintilla_SOURCE_DIR}/src/*.h"
    "${scintilla_SOURCE_DIR}/src/*.cxx"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEditBase/*.h"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEditBase/*.cpp"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/*.h"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/*.cpp"
)

target_sources(scintilla-qt
PRIVATE
    ${SRCS}
    "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.h"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEdit/ScintillaEdit.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/scintilla-qregex/QRegularExpressionSearch.cxx"
)

add_dependencies(scintilla-qt scintilla-widgetgen)

target_compile_definitions(scintilla-qt
PRIVATE
    -DSCINTILLA_QT=1 -DMAKING_LIBRARY=1 -DSCI_OWNREGEX
)

target_include_directories(scintilla-qt
PUBLIC
    "${scintilla_SOURCE_DIR}/include"
    "${scintilla_SOURCE_DIR}/src"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEditBase"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEdit"
PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/scintilla-qregex"
)

target_link_libraries(scintilla-qt
PUBLIC
    Qt::Widgets Qt::Core5Compat
)

# Create an ALIAS target for modern CMake consumption
if(NOT TARGET Scintilla::Scintilla)
    add_library(Scintilla::Scintilla ALIAS scintilla-qt)
endif()

# Set variables for find_package (for backward compatibility)
set(Scintilla_FOUND TRUE)
set(Scintilla_LIBRARIES Scintilla::Scintilla) # Point to the ALIAS target
set(Scintilla_INCLUDE_DIRS
    "${scintilla_SOURCE_DIR}/include"
    "${scintilla_SOURCE_DIR}/src"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEditBase"
    "${scintilla_SOURCE_DIR}/qt/ScintillaEdit"
)

# Mark as advanced so it doesn't show up in GUI by default
mark_as_advanced(Scintilla_FOUND Scintilla_LIBRARIES Scintilla_INCLUDE_DIRS)

install(TARGETS scintilla-qt 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
