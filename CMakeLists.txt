# SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>

cmake_minimum_required(VERSION 3.24)

project(pineapple-notepad VERSION 1.5.0)
set(PROJECT_VERSION_SUFFIX "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON) # kconfig generated header file is in binary dir

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}) # for copy subproject dll to binary folder. this might not be the correct approach tho

cmake_host_system_information(RESULT DISTRO_NAME QUERY DISTRIB_ID)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
include(KDEInstallDirs)
include(ECMAddAppIcon)
endif()

find_package(Scintilla REQUIRED)
find_package(Lexilla REQUIRED)
find_package(Monocypher REQUIRED)

find_package(Qt6 6.7 REQUIRED
    COMPONENTS Widgets Network Core5Compat LinguistTools
)

find_package(KF6 6.6 REQUIRED
    COMPONENTS XmlGui ConfigWidgets CoreAddons I18n IconThemes Codecs WidgetsAddons SyntaxHighlighting Sonnet
)

if (NOT DEFINED CACHE{QT_FEATURE_winsdkicu} AND NOT DEFINED CACHE{QT_FEATURE_icu})
    message(FATAL_ERROR "Qt's ICU feature is NOT enabled")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
# on deepin, if we include these earlier, it will cause the following error:
# Targets not yet defined: CorePrivate
# on Arch Linux, if we include these earlier, it will cause the following error:
# qt_generate_foreign_qml_types() is only available in Qt 6.
include(KDEInstallDirs)
include(ECMAddAppIcon)
endif()

option(SPONSOR_EDITION
    "Build as Sponsor Edition - all sponsor-gated features are unlocked without requiring a license. Intended for builds distributed to specific sponsors; not for public releases."
    ON)
if(SPONSOR_EDITION)
    add_compile_definitions(SPONSOR_EDITION)
endif()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(keygen)
add_subdirectory(po)
add_subdirectory(dist)

if(MSVC)
    add_subdirectory(shell-extension)
endif()

option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
