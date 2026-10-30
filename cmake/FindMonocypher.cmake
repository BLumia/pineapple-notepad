# FindMonocypher.cmake
#
# Fetches Monocypher (https://monocypher.org/) via FetchContent and
# defines the `monocypher` STATIC library target. The download source
# (online url or the local offline/ archive) is decided centrally in
# cmake/PnDependencies.cmake based on dependencies.json.
#
# Both the native EdDSA (BLAKE2b) variant and the optional RFC 8032 Ed25519
# (SHA-512) variant are compiled in. The project uses crypto_ed25519_* for
# license signing/verification to stay interoperable with standard Ed25519
# libraries in other languages (Go crypto/ed25519, JS @noble/ed25519, etc.).
#
# Once done this will define
#   Monocypher_FOUND        - True if Monocypher was prepared
#   Monocypher_LIBRARIES    - monocypher (static lib target)
#   Monocypher_INCLUDE_DIRS - ${monocypher_SOURCE_DIR}/src

if(Monocypher_FOUND)
    return()
endif()

include(FetchContent)
include(PnDependencies)

# All version/url/hash metadata lives in dependencies.json; offline
# archives under offline/ are picked up automatically when present.
pn_declare_dependency(monocypher)

FetchContent_MakeAvailable(monocypher)

add_library(monocypher STATIC)
# monocypher is pure C; the project-wide AUTOMOC/AUTOUIC/AUTORCC have
# nothing to do here and would otherwise emit empty generated units.
set_target_properties(monocypher PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
target_sources(monocypher PRIVATE
    "${monocypher_SOURCE_DIR}/src/monocypher.c"
    "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c"
)
target_include_directories(monocypher PUBLIC
    "${monocypher_SOURCE_DIR}/src"
    "${monocypher_SOURCE_DIR}/src/optional"
)

# monocypher.c is third-party C code we do not want to fail our project's
# stricter warning settings.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|MSVC")
    target_compile_options(monocypher PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-w>
        $<$<C_COMPILER_ID:MSVC>:/W0>
    )
endif()

set_target_properties(monocypher PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

set(Monocypher_FOUND TRUE)
set(Monocypher_LIBRARIES monocypher)
set(Monocypher_INCLUDE_DIRS "${monocypher_SOURCE_DIR}/src")
mark_as_advanced(Monocypher_FOUND Monocypher_LIBRARIES Monocypher_INCLUDE_DIRS)
