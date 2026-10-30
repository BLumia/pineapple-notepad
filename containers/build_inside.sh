#!/usr/bin/env bash
set -e

BUILD_DIR=build-container
INSTALL_DIR=install

rm -rf $BUILD_DIR $INSTALL_DIR

# Offline builds need no special handling here anymore: CMake
# (cmake/PnDependencies.cmake) automatically consumes the archives under
# offline/ when that directory exists, and falls back to online
# FetchContent sources otherwise.
cmake -S . -B $BUILD_DIR \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

cmake --build $BUILD_DIR -j$(nproc)

cd $BUILD_DIR
cpack -G DEB

