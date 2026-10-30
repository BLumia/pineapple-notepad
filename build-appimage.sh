#!/usr/bin/env bash
#
# SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
#
# Build pineapple-notepad and package it as an x86_64 AppImage.
#
# Requirements (host system):
#   - cmake (>= 3.27 for --strip), ninja, g++, pkg-config
#   - Qt6 + KF6 development packages (same set as linglong.yaml)
#   - The two AppImages below must exist in the project root:
#       linuxdeploy-x86_64.AppImage
#       linuxdeploy-plugin-qt-x86_64.AppImage
#
# Usage:
#   ./build-appimage.sh [--incremental]
#
# Options:
#   --incremental, -i   Reuse the existing build-appimage/ build directory
#                      (skip the clean build step). The AppDir and output
#                      AppImage are always regenerated. Useful when only
#                      the packaging steps below have changed.
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

BUILD_DIR="$PROJECT_ROOT/build-appimage"
APPDIR="$PROJECT_ROOT/AppDir"
OUTPUT_DIR="$PROJECT_ROOT/output/appimage"
APPIMAGE_NAME="pineapple-notepad-x86_64.AppImage"

LINUXDEPLOY="$PROJECT_ROOT/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_PLUGIN_QT="$PROJECT_ROOT/linuxdeploy-plugin-qt-x86_64.AppImage"

INCREMENTAL=0

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
        --incremental|-i)
            INCREMENTAL=1
            shift
            ;;
        -h|--help)
            sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            die "Unknown argument: $1 (use --incremental/-i or --help)"
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

COLOR_TTY=$([ -t 1 ] && echo 1 || echo 0)
if [ "$COLOR_TTY" = 1 ]; then
    C_INFO=$'\033[0;34m'; C_WARN=$'\033[1;33m'
    C_ERR=$'\033[0;31m';  C_RST=$'\033[0m'
else
    C_INFO=''; C_WARN=''; C_ERR=''; C_RST=''
fi
log()  { printf "%s[INFO]%s  %s\n" "$C_INFO" "$C_RST" "$*" >&2; }
warn() { printf "%s[WARN]%s  %s\n" "$C_WARN" "$C_RST" "$*" >&2; }
err()  { printf "%s[ERROR]%s %s\n" "$C_ERR"  "$C_RST" "$*" >&2; }
die()  { err "$@"; exit 1; }

# ---------------------------------------------------------------------------
# Pre-flight checks
# ---------------------------------------------------------------------------

command -v cmake  >/dev/null 2>&1 || die "cmake not found in PATH"
command -v ninja  >/dev/null 2>&1 || die "ninja not found in PATH"
command -v qmake6 >/dev/null 2>&1 || die "qmake6 not found in PATH (need Qt6 dev packages)"

[ -x "$LINUXDEPLOY" ]           || die "linuxdeploy not found or not executable: $LINUXDEPLOY"
[ -x "$LINUXDEPLOY_PLUGIN_QT" ] || die "linuxdeploy-plugin-qt not found or not executable: $LINUXDEPLOY_PLUGIN_QT"

QT_PLUGINS_DIR="$(qmake6 -query QT_INSTALL_PLUGINS)"
[ -d "$QT_PLUGINS_DIR" ] || die "Qt6 plugins directory not found: $QT_PLUGINS_DIR"

# linuxdeploy-plugin-qt calls "qmake" (not "qmake6"). On distributions using
# qtchooser (e.g. Debian/Ubuntu), the plain "qmake" wrapper may not have a
# default Qt version selected and fails with "could not find a Qt installation
# of ''". Point the plugin at qmake6 explicitly via the $QMAKE env var.
if command -v qmake6 >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake6)"
fi

BREEZE6_SO="$QT_PLUGINS_DIR/styles/breeze6.so"
KICONENGINEPLUGIN_SO="$QT_PLUGINS_DIR/kiconthemes6/iconengines/KIconEnginePlugin.so"

log "Project root : $PROJECT_ROOT"
log "Qt6 plugins  : $QT_PLUGINS_DIR"
log "Build dir     : $BUILD_DIR"
log "AppDir        : $APPDIR"
log "Output dir    : $OUTPUT_DIR"
log "Mode          : $( [ "$INCREMENTAL" = 1 ] && echo incremental || echo clean )"

# ---------------------------------------------------------------------------
# Stage 1: CMake build
# ---------------------------------------------------------------------------
#
# Default mode is a full clean build (like build_inside.sh). With
# --incremental, the existing build-appimage/ directory is reused so that
# e.g. iterating on packaging steps does not force a complete rebuild.
# The AppDir and output AppImage are always regenerated from scratch.

log "Cleaning AppDir and output..."
rm -rf "$APPDIR" "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

if [ "$INCREMENTAL" = 1 ]; then
    if [ -d "$BUILD_DIR" ] && [ -f "$BUILD_DIR/build.ninja" ]; then
        log "Incremental mode: reusing existing build dir $BUILD_DIR"
    else
        warn "Incremental mode requested but $BUILD_DIR is not a valid Ninja"
        warn "  build tree; falling back to a clean configure."
        rm -rf "$BUILD_DIR"
    fi
fi

# (Re)configure when the build dir is missing or empty.
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    log "Configuring CMake (Ninja, Release)..."
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTING=OFF
fi

log "Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

log "Installing into AppDir..."
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr" --strip

# ---------------------------------------------------------------------------
# Stage 2: prepare AppDir metadata for linuxdeploy
# ---------------------------------------------------------------------------

log "Copying desktop file and icon to AppDir root..."
cp "$PROJECT_ROOT/dist/net.blumia.pineapple-notepad.desktop" "$APPDIR/"
cp "$PROJECT_ROOT/icons/pineapple-notepad.svg" \
   "$APPDIR/net.blumia.pineapple-notepad.svg"

# Pre-copy the KIconEnginePlugin icon engine plugin. It lives under the
# non-standard "kiconthemes6/iconengines" subdirectory of the Qt plugins
# folder, which linuxdeploy-plugin-qt does not scan. Copying it *before*
# running linuxdeploy ensures its (KF6) dependencies are bundled as well.
if [ -f "$KICONENGINEPLUGIN_SO" ]; then
    log "Pre-copying KIconEnginePlugin.so into AppDir..."
    mkdir -p "$APPDIR/usr/plugins/kiconthemes6/iconengines"
    cp "$KICONENGINEPLUGIN_SO" "$APPDIR/usr/plugins/kiconthemes6/iconengines/"
else
    warn "KIconEnginePlugin.so not found at: $KICONENGINEPLUGIN_SO"
    warn "  -> Icon color rendering may be incorrect in the resulting AppImage."
fi

# ---------------------------------------------------------------------------
# Stage 3: run linuxdeploy (bundles Qt6/KF6 libs + Qt plugins, makes AppImage)
# ---------------------------------------------------------------------------

# linuxdeploy looks for a companion binary named "linuxdeploy-plugin-qt" in
# PATH. Our file is named "linuxdeploy-plugin-qt-x86_64.AppImage", so we
# create a temporary symlink under a temp dir added to PATH.
PLUGIN_TMPDIR="$(mktemp -d)"
trap 'rm -rf "$PLUGIN_TMPDIR"' EXIT
ln -sf "$LINUXDEPLOY_PLUGIN_QT" "$PLUGIN_TMPDIR/linuxdeploy-plugin-qt"
export PATH="$PLUGIN_TMPDIR:$PATH"

# Force deployment of the "styles" Qt plugin folder so that breeze6.so (the
# Breeze Qt Widgets style used by the app) is bundled even though it is only
# loaded at runtime and not linked against directly.
export EXTRA_QT_PLUGINS="styles"

# Deploy the Wayland QPA platform plugins in addition to the default XCB one.
# Without these, the AppImage cannot run natively on Wayland sessions and
# would fall back to XCB via XWayland (or fail to start if no X server is
# available). linuxdeploy-plugin-qt also auto-bundles the matching Wayland
# client side plugins (wayland-shell-integration, wayland-decoration-client)
# once the Wayland platform plugins are requested here.
export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"

# Tell linuxdeploy where to write the final AppImage.
export OUTPUT="$OUTPUT_DIR/$APPIMAGE_NAME"

log "Running linuxdeploy with Qt plugin (bundles Qt6/KF6 libs and Qt"
log "  plugins, including breeze6.so + Wayland QPA plugins)..."
"$LINUXDEPLOY" --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$APPDIR/net.blumia.pineapple-notepad.desktop" \
    --icon-file "$APPDIR/net.blumia.pineapple-notepad.svg"

# ---------------------------------------------------------------------------
# Stage 4: resolve transitive deps of runtime-loaded plugins
# ---------------------------------------------------------------------------
#
# linuxdeploy-plugin-qt deploys the Qt plugins referenced via
# EXTRA_QT_PLUGINS (e.g. breeze6.so) but does not always recurse into
# their own KF6/Qt6 dependencies (libKF6WindowSystem, libQt6Quick, ...).
# Run linuxdeploy again with --deploy-deps-only over the whole AppDir so
# any still-unresolved library pulled in as a plugin gets its dependencies
# bundled and rpath-adjusted. This is a no-op for libraries that are
# already fully resolved.
log "Resolving transitive deps of all bundled ELF files (esp. plugins)..."
"$LINUXDEPLOY" --appdir "$APPDIR" \
    --deploy-deps-only "$APPDIR/usr/plugins" \
    --desktop-file "$APPDIR/net.blumia.pineapple-notepad.desktop" \
    --icon-file "$APPDIR/net.blumia.pineapple-notepad.svg"

# ---------------------------------------------------------------------------
# Stage 5: fallback - manually bundle breeze6.so if it was missed
# ---------------------------------------------------------------------------

if [ ! -f "$APPDIR/usr/plugins/styles/breeze6.so" ]; then
    if [ -f "$BREEZE6_SO" ]; then
        warn "breeze6.so was not bundled by linuxdeploy-plugin-qt; copying manually..."
        mkdir -p "$APPDIR/usr/plugins/styles"
        cp "$BREEZE6_SO" "$APPDIR/usr/plugins/styles/"
        log "Re-running linuxdeploy --deploy-deps-only to resolve the new"
        log "  plugin's deps..."
        "$LINUXDEPLOY" --appdir "$APPDIR" \
            --deploy-deps-only "$APPDIR/usr/plugins/styles" \
            --desktop-file "$APPDIR/net.blumia.pineapple-notepad.desktop" \
            --icon-file "$APPDIR/net.blumia.pineapple-notepad.svg"
    else
        warn "breeze6.so not found at: $BREEZE6_SO; the Breeze style will"
        warn "  not be available in the resulting AppImage."
    fi
fi

# ---------------------------------------------------------------------------
# Stage 6: generate the final AppImage
# ---------------------------------------------------------------------------
#
# Kept separate from the dependency-resolution passes so that a missing
# transitive dep can be fixed above without regenerating the AppImage
# each time.
log "Generating final AppImage..."
"$LINUXDEPLOY" --appdir "$APPDIR" \
    --output appimage \
    --desktop-file "$APPDIR/net.blumia.pineapple-notepad.desktop" \
    --icon-file "$APPDIR/net.blumia.pineapple-notepad.svg"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------

RESULT="$OUTPUT_DIR/$APPIMAGE_NAME"
if [ -f "$RESULT" ]; then
    log "AppImage created successfully:"
    log "  $RESULT ($(du -h "$RESULT" | cut -f1))"
else
    die "Expected AppImage was not produced: $RESULT"
fi
