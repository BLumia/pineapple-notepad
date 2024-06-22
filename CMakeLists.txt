# SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>

cmake_minimum_required(VERSION 3.9.5)

project(pineapple-notepad)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON) # kconfig generated header file is in binary dir

find_package(Qt6 6.4 REQUIRED
    COMPONENTS Widgets
)

find_package(KF6 6.3 REQUIRED
    COMPONENTS XmlGui ConfigWidgets CoreAddons I18n IconThemes
)

add_executable(pineapple-notepad
    generalsettings.ui)

target_sources(pineapple-notepad
PRIVATE
    main.cpp
    mainwindow.cpp mainwindow.h
    generalsettings.ui

    appsettings.kcfg
)

qt_add_resources(pineapple-notepad xmlgui.qrc
    PREFIX "/kxmlgui5/pineapple-notepad"
    FILES pineapple-notepadui.rc
)

kconfig_add_kcfg_files(pineapple-notepad GENERATE_MOC appsettings.kcfgc)

target_link_libraries(pineapple-notepad
PRIVATE
    Qt6::Widgets
    KF6::XmlGui
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigWidgets
    KF6::IconThemes
    qscintilla2_qt6
)
