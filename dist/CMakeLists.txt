if (WIN32)
    install(
        FILES passoc/pnotepad.pacfg
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME default-assoc.pacfg
    )

    install(
        FILES
            passoc/generic-text.ico
            passoc/generic-xml.ico
            passoc/generic-src.ico
            passoc/generic-script.ico
            passoc/json.ico
            passoc/md.ico
            passoc/yaml.ico
            passoc/toml.ico
        DESTINATION "${CMAKE_INSTALL_BINDIR}/icons"
    )
endif()

if (LINUX)
    # install icon
    install (
        FILES ../icons/pineapple-notepad.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME net.blumia.pineapple-notepad.svg
    )

    # install shortcut
    install(
        FILES net.blumia.pineapple-notepad.desktop
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
    )

    # install app metadata file for appstream (and some other stuff using this metadata like snapcraft)
    install (
        FILES net.blumia.pineapple-notepad.metainfo.xml
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
    )
endif()

# CPACK: General Settings
set (CPACK_GENERATOR "TBZ2")
set (CPACK_PACKAGE_NAME "pineapple-notepad")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Scintilla-based Text Editor")
set (CPACK_PACKAGE_VENDOR "Gary Wang")
set (CPACK_PACKAGE_CONTACT "https://github.com/BLumia/pineapple-notepad/issues/")

if (LINUX)
    set (CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set (CPACK_DEBIAN_PACKAGE_RECOMMENDS "kde-style-breeze, libkf6iconthemes-bin")
endif()

if (APPLE)
    set_target_properties(pineapple-notepad PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Pineapple Notepad"
        MACOSX_BUNDLE_GUI_IDENTIFIER net.blumia.pineapple-notepad
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif()

include(CPack)