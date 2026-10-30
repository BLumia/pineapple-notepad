FROM ubuntu:26.04

RUN rm -f /etc/apt/sources.list.d/ubuntu.sources && \
    printf "Types: deb\n\
URIs: http://mirrors.cernet.edu.cn/ubuntu\n\
Suites: resolute resolute-updates plucky-backports\n\
Components: main restricted universe multiverse\n\
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n\
\n\
Types: deb\n\
URIs: http://mirrors.cernet.edu.cn/ubuntu\n\
Suites: resolute-security\n\
Components: main restricted universe multiverse\n\
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n" \
> /etc/apt/sources.list.d/ubuntu.sources

ENV DEBIAN_FRONTEND=noninteractive

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
