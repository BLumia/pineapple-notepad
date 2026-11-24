FROM docker.io/linuxdeepin/deepin:crimson

RUN dpkg --add-architecture arm64 && apt-get update && \
    apt-get install -y -o Dpkg::Options::="--force-overwrite" \
    file \
    cmake \
    extra-cmake-modules \
    ninja-build \
    qt6-base-dev-tools \
    g++-aarch64-linux-gnu \
    qt6-base-dev:arm64 \
    qt6-5compat-dev:arm64 \
    libkf6coreaddons-dev:arm64 \
    libkf6i18n-dev:arm64 \
    libkf6xmlgui-dev:arm64 \
    libkf6iconthemes-dev:arm64 \
    libkf6widgetsaddons-dev:arm64 \
    libkf6configwidgets-dev:arm64 \
    libkf6codecs-dev:arm64 \
    libkf6syntaxhighlighting-dev:arm64 \
    libkf6sonnet-dev:arm64 \
    git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
