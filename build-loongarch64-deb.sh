#!/usr/bin/env bash
#
# build-loongarch64-deb.sh
# Cross-build .deb package for loongarch64 using CMake + CPack
#
# Prerequisites on amd64 host:
#   sudo apt install cmake ninja-build g++-loongarch64-linux-gnu \
#     qt6-base-dev-tools extra-cmake-modules
#   sudo dpkg --add-architecture loong64
#   sudo apt update
#   sudo apt install -o Dpkg::Options::="--force-overwrite" \
#     qt6-base-dev:loong64 qt6-5compat-dev:loong64 \
#     libkf6coreaddons-dev:loong64 \
#     libkf6doctools-dev:loong64 libkf6i18n-dev:loong64 \
#     libkf6iconthemes-dev:loong64 libkf6widgetsaddons-dev:loong64 \
#     libkf6xmlgui-dev:loong64 libkf6configwidgets-dev:loong64

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}"
BUILD_DIR="${PROJECT_DIR}/build-loong64"
PACKAGE_DIR="${BUILD_DIR}/_CPack_Packages"
HOST_ARCH="$(dpkg --print-architecture)"
GCC_TRIPLE="loongarch64-linux-gnu"
DPKG_TARGET_ARCH="loong64"
TARGET_MULTIARCH="${DPKG_TARGET_ARCH}-linux-gnu"

COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_RESET='\033[0m'

log_info()  { echo -e "${COLOR_GREEN}[INFO]${COLOR_RESET} $*"; }
log_warn()  { echo -e "${COLOR_YELLOW}[WARN]${COLOR_RESET} $*"; }
log_error() { echo -e "${COLOR_RED}[ERROR]${COLOR_RESET} $*"; }

die() { log_error "$@"; exit 1; }

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Cross-build .deb package for loongarch64 on an amd64 host
using CMake cross-compile + CPack.

Options:
  -h                    Show this help message
  --skip-target-check   Skip loong64 Qt6 library check (for testing)

Prerequisites:
  g++-loongarch64-linux-gnu, qt6-base-dev-tools (host), and loong64
  Qt6/KF6 dev libraries must be installed.
  See the header comments in this script for the full install commands.
EOF
}

check_host_arch() {
    if [ "${HOST_ARCH}" != "amd64" ]; then
        die "This script is designed to run on an amd64 host, but current arch is ${HOST_ARCH}."
    fi
}

check_prerequisites() {
    local missing=()

    for cmd in cmake ninja dpkg; do
        if ! command -v "$cmd" &>/dev/null; then
            missing+=("$cmd")
        fi
    done

    if [ ${#missing[@]} -gt 0 ]; then
        die "Missing required commands: ${missing[*]}. Install: sudo apt install cmake ninja-build dpkg"
    fi

    if [ ! -f "/usr/bin/${GCC_TRIPLE}-g++" ]; then
        die "Cross-compiler not found. Install: sudo apt install g++-${GCC_TRIPLE}"
    fi

    if ! command -v qtpaths6 &>/dev/null; then
        die "qtpaths6 not found. Install: sudo apt install qt6-base-dev-tools"
    fi

    local target_qt_dir="/usr/lib/${GCC_TRIPLE}/cmake/Qt6"
    if [ ! -d "${target_qt_dir}" ]; then
        log_warn "Target Qt6 cmake dir not found: ${target_qt_dir}"
        log_warn "Install loong64 Qt6/KF6 dev libraries:"
        log_warn "  sudo dpkg --add-architecture loong64"
        log_warn "  sudo apt update"
        log_warn "  sudo apt install qt6-base-dev:loong64 ..."
        if [ "${SKIP_TARGET_CHECK:-0}" = "0" ]; then
            die "Target Qt6 libraries are required. Use --skip-target-check to attempt build anyway."
        fi
        log_warn "Continuing anyway (--skip-target-check)..."
    fi
}

build() {
    log_info "=== Cross-compiling for loongarch64 (dpkg arch: ${DPKG_TARGET_ARCH}) ==="

    rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"

    local host_multiarch="${HOST_ARCH}-linux-gnu"
    local target_lib_dir="/usr/lib/${GCC_TRIPLE}"
    local host_lib_dir="/usr/lib/${host_multiarch}"

    local toolchain_file="${BUILD_DIR}/toolchain-loong64.cmake"
    cat > "${toolchain_file}" <<TOOLCHAIN_EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR loongarch64)
set(CMAKE_C_COMPILER   ${GCC_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${GCC_TRIPLE}-g++)

set(CMAKE_FIND_ROOT_PATH "${target_lib_dir};/usr" CACHE PATH "Find root path")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(ECM_DIR "/usr/share/ECM/cmake" CACHE PATH "ECM cmake config dir")

set(_qt6_host_tools_cmake_dir "/usr/lib/${host_multiarch}/cmake/Qt6CoreTools")
if(EXISTS "\${_qt6_host_tools_cmake_dir}/Qt6CoreToolsTargets.cmake")
    set(Qt6CoreTools_DIR "\${_qt6_host_tools_cmake_dir}" CACHE PATH "" FORCE)
    message(STATUS "Cross-compile: using host Qt6CoreTools from \${_qt6_host_tools_cmake_dir}")
endif()
TOOLCHAIN_EOF

    local cmake_args=(
        -S "${PROJECT_DIR}"
        -B "${BUILD_DIR}"
        -G "Ninja"
        -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=/usr
        -DBUILD_TESTING=OFF
        -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
        -DECM_DIR="/usr/share/ECM/cmake"
        -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="${DPKG_TARGET_ARCH}"
    )

    log_info "Configuring with CMake..."
    cmake "${cmake_args[@]}"

    log_info "Building..."
    cmake --build "${BUILD_DIR}" -j"$(nproc)"

    log_info "Packaging with CPack..."
    cpack \
        --config "${BUILD_DIR}/CPackConfig.cmake" \
        -G DEB \
        -B "${PACKAGE_DIR}" \
        --verbose

    log_info "=== Build complete ==="
    log_info "Looking for generated .deb files..."
    find "${PACKAGE_DIR}" -maxdepth 1 -name "*.deb" -type f -exec ls -lh {} \;
}

main() {
    while getopts "h-:" opt; do
        case "${opt}" in
            h) usage; exit 0 ;;
            -)
                case "${OPTARG}" in
                    skip-target-check) SKIP_TARGET_CHECK=1 ;;
                    *) usage; exit 1 ;;
                esac
                ;;
            *) usage; exit 1 ;;
        esac
    done

    check_host_arch
    check_prerequisites
    build
}

main "$@"
