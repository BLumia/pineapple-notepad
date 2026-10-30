# SPDX-License-Identifier: MIT
# License Key Generator - standalone Qt Widgets tool built alongside the
# main project (see also tools/ for the same pattern).
#
# Produces the `keygen` executable, which offers three tabs:
#   1. Generate a public key from a 32-byte hex seed
#   2. Sign a license file (.lic) using the locally saved private key
#   3. Verify a .lic file against the locally saved public key

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets)

add_executable(keygen WIN32)

set_target_properties(keygen PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_sources(keygen PRIVATE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    keygentab.cpp
    keygentab.h
    licensetab.cpp
    licensetab.h
    verifytab.cpp
    verifytab.h
    ${CMAKE_SOURCE_DIR}/lib/licensecrypto.cpp
    ${CMAKE_SOURCE_DIR}/lib/licensecrypto.h
    ${CMAKE_SOURCE_DIR}/lib/licensefile.cpp
    ${CMAKE_SOURCE_DIR}/lib/licensefile.h
    ${CMAKE_SOURCE_DIR}/lib/stringhelpers.cpp
    ${CMAKE_SOURCE_DIR}/lib/stringhelpers.h
)

target_include_directories(keygen PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(keygen PRIVATE
    Qt6::Widgets
    monocypher
)

#install(TARGETS keygen ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
