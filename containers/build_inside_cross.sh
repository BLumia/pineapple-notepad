#!/usr/bin/env bash
#
# build_inside_cross.sh
# Cross-build .deb package for a foreign architecture inside a container
# (amd64 host).
#
# Target information is provided via environment variables:
#   CROSS_TRIPLE           GCC cross toolchain triple, e.g. aarch64-linux-gnu
#   CROSS_DPKG_ARCH        dpkg architecture of the target, e.g. arm64
#   CROSS_CMAKE_PROCESSOR  CMAKE_SYSTEM_PROCESSOR of the target, e.g. aarch64
#
set -e

: "${CROSS_TRIPLE:?CROSS_TRIPLE is not set}"
: "${CROSS_DPKG_ARCH:?CROSS_DPKG_ARCH is not set}"
: "${CROSS_CMAKE_PROCESSOR:?CROSS_CMAKE_PROCESSOR is not set}"

BUILD_DIR=build-container
GCC_TRIPLE=${CROSS_TRIPLE}
DPKG_TARGET_ARCH=${CROSS_DPKG_ARCH}
CMAKE_SYSTEM_PROCESSOR=${CROSS_CMAKE_PROCESSOR}
HOST_MULTIARCH=x86_64-linux-gnu

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Generate cmake toolchain file for cross-compilation
cat > ${BUILD_DIR}/toolchain-${DPKG_TARGET_ARCH}.cmake <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
set(CMAKE_C_COMPILER   ${GCC_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${GCC_TRIPLE}-g++)

set(CMAKE_FIND_ROOT_PATH "/usr/lib/${GCC_TRIPLE};/usr" CACHE PATH "Find root path")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(ECM_DIR "/usr/share/ECM/cmake" CACHE PATH "ECM cmake config dir")

set(_qt6_host_tools_cmake_dir "/usr/lib/${HOST_MULTIARCH}/cmake/Qt6CoreTools")
if(EXISTS "\${_qt6_host_tools_cmake_dir}/Qt6CoreToolsTargets.cmake")
    set(Qt6CoreTools_DIR "\${_qt6_host_tools_cmake_dir}" CACHE PATH "" FORCE)
    message(STATUS "Cross-compile: using host Qt6CoreTools from \${_qt6_host_tools_cmake_dir}")
endif()
EOF

# Offline builds need no special handling here anymore: CMake
# (cmake/PnDependencies.cmake) automatically consumes the archives under
# offline/ when that directory exists, and falls back to online
# FetchContent sources otherwise.
cmake -S . -B $BUILD_DIR \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=${BUILD_DIR}/toolchain-${DPKG_TARGET_ARCH}.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=OFF \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
    -DECM_DIR="/usr/share/ECM/cmake" \
    -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE=${DPKG_TARGET_ARCH}

cmake --build $BUILD_DIR -j$(nproc)

cd $BUILD_DIR
cpack -G DEB
