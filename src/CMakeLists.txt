add_executable(pineapple-notepad WIN32 MACOSX_BUNDLE)

# Third-party dependency versions for the About dialog, generated from
# dependencies.json so a version bump there is the only change needed.
include(PnDependencies)
pn_get_version(scintilla  PN_DEP_VERSION_SCINTILLA)
pn_get_version(lexilla   PN_DEP_VERSION_LEXILLA)
pn_get_version(monocypher PN_DEP_VERSION_MONOCYPHER)
pn_get_version(qtads      PN_DEP_VERSION_QTADS)
configure_file(dependency_versions.h.in dependency_versions.h)

set_target_properties(pineapple-notepad PROPERTIES
    OUTPUT_NAME "pnotepad"
)

ecm_add_app_icon(pineapple-notepad
    ICONS
        ../icons/64-pineapple-notepad.png
        ../icons/256-pineapple-notepad.png
)

add_dependencies(pineapple-notepad scintilla-qt)

qt_add_translations(pineapple-notepad
RESOURCE_PREFIX "/i18n"
LUPDATE_OPTIONS -no-ui-lines -locations none # -no-obsolete
TS_FILES
    translations/pineapple-notepad_en.ts
    translations/pineapple-notepad_zh_CN.ts
)

target_compile_definitions(pineapple-notepad
PRIVATE
    PINEAPPLE_NOTEPAD_VERSION_STR="${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}"
)

target_sources(pineapple-notepad
PRIVATE
    main.cpp
    mainwindow.cpp mainwindow.h
    editorthemepage.cpp editorthemepage.h
    settingsdialog.cpp settingsdialog.h
    editorpopupconfigdialog.cpp editorpopupconfigdialog.h
    sponsordialog.cpp sponsordialog.h
    sponsordialog.ui
    sessionmanager.cpp sessionmanager.h
    favoritesstore.cpp favoritesstore.h
    editorviewmanager.h
    tabwidget.cpp tabwidget.h
    findreplacedialog.cpp findreplacedialog.h
    findreplacecontroller.cpp findreplacecontroller.h
    findinfilessearcher.cpp findinfilessearcher.h
    findinfinderdialog.cpp findinfinderdialog.h
    searchresultsdock.cpp searchresultsdock.h
    searchresultview.cpp searchresultview.h
    folderworkspacedock.cpp folderworkspacedock.h
    folderworkspacemanager.cpp folderworkspacemanager.h
    documentmap.cpp documentmap.h
    documentlistdock.cpp documentlistdock.h
    quickswitcher.cpp quickswitcher.h
    recenttagproxymodel.cpp recenttagproxymodel.h
    viewzone.cpp viewzone.h
    filechangewatcher.cpp filechangewatcher.h
    gotodialog.cpp gotodialog.h
    rundialog.cpp rundialog.h
    summarydialog.cpp summarydialog.h
    macro.cpp macro.h
    macrorecorder.cpp macrorecorder.h
    macrostep.cpp macrostep.h
    macrorundialog.cpp macrorundialog.h
    macromanagedialog.cpp macromanagedialog.h
    macromanagedialog.ui
    languageselectordialog.ui
    languageselectordialog.cpp languageselectordialog.h
    languagemanagerdialog.ui
    languagemanagerdialog.cpp languagemanagerdialog.h
    userlanguageeditordialog.ui
    userlanguageeditordialog.cpp userlanguageeditordialog.h
    favoritesmanagedialog.ui
    favoritesmanagedialog.cpp favoritesmanagedialog.h
    editorpopupconfigdialog.ui
    editorpopupconfigdialog.cpp editorpopupconfigdialog.h
    singleapplicationmanager.cpp singleapplicationmanager.h
    fileopeneventhandler.cpp fileopeneventhandler.h
    generalsettings.ui
    fontsettings.ui
    editorthemesettings.ui
    indentationsettings.ui
    editingsettings.ui
    highlightssettings.ui
    autocompletionsettings.ui
    marginsettings.ui
    backupsettings.ui
    filewatchsettings.ui
    findreplacedialog.ui
    findinfinderdialog.ui
    searchsettings.ui
    searchresultsdock.ui
    folderworkspacedock.ui
    documentlistdock.ui
    gotodialog.ui
    rundialog.ui
    summarydialog.ui
    appsettings.kcfg
)

target_include_directories(pineapple-notepad
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

qt_add_resources(pineapple-notepad xmlgui.qrc
    PREFIX "/kxmlgui5/pineapple-notepad"
    FILES pineapple-notepadui.rc
)

qt_add_resources(pineapple-notepad icons.qrc
    PREFIX "/icons"
    FILES
        ../icons/pineapple-notepad.svg
        ../icons/invisible-char.svg
        ../icons/indent-guide.svg
        ../icons/document-close-all.svg
        ../icons/document-save-modified.svg
        ../icons/macro-record.svg
        ../icons/macro-record-stop.svg
        ../icons/macro-save.svg
        ../icons/macro-playback-start.svg
        ../icons/macro-playback-pause.svg
        ../icons/macro-playback-repeat.svg
)

qt_add_resources(pineapple-notepad license.qrc
    PREFIX "/"
    FILES
        ../LICENSE
)

kconfig_add_kcfg_files(pineapple-notepad GENERATE_MOC appsettings.kcfgc)

target_link_libraries(pineapple-notepad
PRIVATE
    pineapple-notepad-lib
    Qt6::Widgets
    Qt6::Core5Compat
    Qt6::Network
    KF6::XmlGui
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigWidgets
    KF6::IconThemes
    KF6::Codecs
    KF6::WidgetsAddons
    KF6::SonnetUi
)

install(TARGETS pineapple-notepad ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
