FROM docker.io/linuxdeepin/deepin:crimson

RUN apt-get update && apt-get install -y \
    file \
    build-essential \
    cmake \
    extra-cmake-modules \
    ninja-build \
    qt6-base-dev \
    qt6-5compat-dev \
    qt6-tools-dev \
    libkf6i18n-dev \
    libkf6xmlgui-dev \
    libkf6iconthemes-dev \
    libkf6syntaxhighlighting-dev \
    libkf6sonnet-dev \
    git

WORKDIR /workspace
