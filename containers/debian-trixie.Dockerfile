FROM debian:trixie

RUN rm -f /etc/apt/sources.list.d/debian.sources && \
    printf "Types: deb\n\
URIs: http://mirrors.cernet.edu.cn/debian\n\
Suites: trixie trixie-updates\n\
Components: main contrib non-free non-free-firmware\n\
\n\
Types: deb\n\
URIs: http://mirrors.cernet.edu.cn/debian-security\n\
Suites: trixie-security\n\
Components: main contrib non-free non-free-firmware\n" \
> /etc/apt/sources.list.d/debian.sources

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
