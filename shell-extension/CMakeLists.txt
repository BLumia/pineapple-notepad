if(NOT WIN32)
    message(FATAL_ERROR "This shell extension targets Windows only.")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "This shell extension requires MSVC.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(pnotepadshellext SHARED
    src/ExplorerCommandHandler.cpp
    src/dllmain.cpp
    src/resource.rc
)

target_include_directories(pnotepadshellext PRIVATE src)

target_compile_definitions(pnotepadshellext PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

target_compile_options(pnotepadshellext PRIVATE /W4 /permissive-)

target_link_libraries(pnotepadshellext PRIVATE
    shlwapi
    shell32
    ole32
    uuid
)

target_link_options(pnotepadshellext PRIVATE
    /EXPORT:DllGetClassObject
    /EXPORT:DllCanUnloadNow
    /EXPORT:DllRegisterServer
    /EXPORT:DllUnregisterServer
)

set_target_properties(pnotepadshellext PROPERTIES
    OUTPUT_NAME "pnotepadshellext"
    PREFIX ""
)

install(TARGETS pnotepadshellext RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
