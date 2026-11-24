#!/usr/bin/env bash
set -e

FORCE_REBUILD=false
DISTROS=()

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --force-rebuild-image)
            FORCE_REBUILD=true
            shift
            ;;
        *)
            DISTROS+=("$1")
            shift
            ;;
    esac
done

# 如果没有指定 distro，使用默认的
if [ ${#DISTROS[@]} -eq 0 ]; then
    DISTROS=(
        "debian-trixie"
        "ubuntu-26.04"
        "deepin-crimson"
        "loong64-cross-deepin-crimson"
        "arm64-cross-deepin-crimson"
        "riscv64-cross-deepin-crimson"
    )
fi

for distro in "${DISTROS[@]}"; do
    echo "=== Building for $distro ==="

    IMAGE="mybuild-$distro"
    DOCKERFILE="containers/$distro.Dockerfile"

    # 根据 distro 名称选择构建脚本
    CROSS_TRIPLE=""
    CROSS_DPKG_ARCH=""
    CROSS_CMAKE_PROCESSOR=""
    if [[ "$distro" == loong64-cross-* ]]; then
        BUILD_SCRIPT="containers/build_inside_cross.sh"
        CROSS_TRIPLE="loongarch64-linux-gnu"
        CROSS_DPKG_ARCH="loong64"
        CROSS_CMAKE_PROCESSOR="loongarch64"
        DEB_DIR="build-container"
    elif [[ "$distro" == arm64-cross-* ]]; then
        BUILD_SCRIPT="containers/build_inside_cross.sh"
        CROSS_TRIPLE="aarch64-linux-gnu"
        CROSS_DPKG_ARCH="arm64"
        CROSS_CMAKE_PROCESSOR="aarch64"
        DEB_DIR="build-container"
    elif [[ "$distro" == riscv64-cross-* ]]; then
        BUILD_SCRIPT="containers/build_inside_cross.sh"
        CROSS_TRIPLE="riscv64-linux-gnu"
        CROSS_DPKG_ARCH="riscv64"
        CROSS_CMAKE_PROCESSOR="riscv64"
        DEB_DIR="build-container"
    else
        BUILD_SCRIPT="containers/build_inside.sh"
        DEB_DIR="build-container"
    fi

    # 检查是否需要重新构建镜像
    NEED_REBUILD=false
    if [ "$FORCE_REBUILD" = true ]; then
        echo "Force rebuild requested, will rebuild image..."
        NEED_REBUILD=true
    elif ! podman image exists $IMAGE 2>/dev/null; then
        echo "Image $IMAGE does not exist, will build..."
        NEED_REBUILD=true
    else
        echo "Image $IMAGE already exists, skipping build (use --force-rebuild-image to force rebuild)"
    fi

    if [ "$NEED_REBUILD" = true ]; then
        podman build -t $IMAGE -f $DOCKERFILE
    fi

    mkdir -p output/$distro

    podman run --rm \
        -v $(pwd):/workspace:Z \
        -v $(pwd)/output/$distro:/output:Z \
        -e CROSS_TRIPLE="$CROSS_TRIPLE" \
        -e CROSS_DPKG_ARCH="$CROSS_DPKG_ARCH" \
        -e CROSS_CMAKE_PROCESSOR="$CROSS_CMAKE_PROCESSOR" \
        $IMAGE \
        bash -c "
            chmod +x $BUILD_SCRIPT
            ./$BUILD_SCRIPT
            cp $DEB_DIR/*.deb /output/
        "
done
