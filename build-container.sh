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
    )
fi

for distro in "${DISTROS[@]}"; do
    echo "=== Building for $distro ==="

    IMAGE="mybuild-$distro"
    DOCKERFILE="containers/$distro.Dockerfile"

    # 根据 distro 名称选择构建脚本
    if [[ "$distro" == loong64-cross-* ]]; then
        BUILD_SCRIPT="containers/build_inside_loong64_cross.sh"
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
        $IMAGE \
        bash -c "
            chmod +x $BUILD_SCRIPT
            ./$BUILD_SCRIPT
            cp $DEB_DIR/*.deb /output/
        "
done
