FROM docker.io/linuxdeepin/deepin:crimson

RUN dpkg --add-architecture riscv64 && apt-get update && \
    apt-get install -y -o Dpkg::Options::="--force-overwrite" \
    file \
    cmake \
    extra-cmake-modules \
    ninja-build \
    qt6-base-dev-tools \
    g++-riscv64-linux-gnu \
    qt6-base-dev:riscv64 \
    qt6-5compat-dev:riscv64 \
    libkf6coreaddons-dev:riscv64 \
    libkf6i18n-dev:riscv64 \
    libkf6xmlgui-dev:riscv64 \
    libkf6iconthemes-dev:riscv64 \
    libkf6widgetsaddons-dev:riscv64 \
    libkf6configwidgets-dev:riscv64 \
    libkf6codecs-dev:riscv64 \
    libkf6syntaxhighlighting-dev:riscv64 \
    libkf6sonnet-dev:riscv64 \
    git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
