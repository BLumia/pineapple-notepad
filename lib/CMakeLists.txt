add_library(pineapple-notepad-lib STATIC)

qt_add_resources(pineapple-notepad-lib config.qrc
    PREFIX "/"
    FILES
        ../language-config.json
        ../theme-extensions.json
)

qt_add_resources(pineapple-notepad-lib themes-addons.qrc
    PREFIX "/org.kde.syntax-highlighting/themes-addons"
    FILES
        ../pineapple-notepad-light.theme
        ../pineapple-notepad-dark.theme
)

target_sources(pineapple-notepad-lib
PRIVATE
    apppaths.cpp apppaths.h
    basestyles.cpp basestyles.h
    document.cpp document.h
    documentmanager.cpp documentmanager.h
    editorpopupconfig.cpp editorpopupconfig.h
    editorviewhelper.cpp editorviewhelper.h
    exporters.cpp exporters.h
    languageconfigstore.cpp languageconfigstore.h
    kshlexer.cpp kshlexer.h
    loglexer.cpp loglexer.h
    searchresultlexer.cpp searchresultlexer.h
    sciedit.cpp sciedit.h
    spellcheckcontroller.cpp spellcheckcontroller.h
    statuslabel.cpp statuslabel.h
    themeconfigstore.cpp themeconfigstore.h
    licensecrypto.cpp licensecrypto.h
    licensefile.cpp licensefile.h
    licenseverifier.cpp licenseverifier.h
    stringhelpers.cpp stringhelpers.h
    textutil.h
)

target_include_directories(pineapple-notepad-lib
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pineapple-notepad-lib
PUBLIC
    Qt6::Widgets
    KF6::XmlGui
    KF6::Codecs
    KF6::SyntaxHighlighting
    KF6::SonnetCore
    Scintilla::Scintilla
    Lexilla::Lexilla
    monocypher
)
