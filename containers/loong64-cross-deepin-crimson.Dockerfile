FROM docker.io/linuxdeepin/deepin:crimson

RUN dpkg --add-architecture loong64 && apt-get update && \
    apt-get install -y -o Dpkg::Options::="--force-overwrite" \
    file \
    cmake \
    extra-cmake-modules \
    ninja-build \
    qt6-base-dev-tools \
    g++-loongarch64-linux-gnu \
    qt6-base-dev:loong64 \
    qt6-5compat-dev:loong64 \
    libkf6coreaddons-dev:loong64 \
    libkf6i18n-dev:loong64 \
    libkf6xmlgui-dev:loong64 \
    libkf6iconthemes-dev:loong64 \
    libkf6widgetsaddons-dev:loong64 \
    libkf6configwidgets-dev:loong64 \
    libkf6codecs-dev:loong64 \
    libkf6syntaxhighlighting-dev:loong64 \
    libkf6sonnet-dev:loong64 \
    git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
