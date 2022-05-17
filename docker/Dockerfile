FROM ubuntu:22.04 AS base

ENV TZ=Europe/Warsaw

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

ENV DEBIAN_FRONTEND="noninteractive"

RUN apt update && \
    apt install -y build-essential cmake libxkbcommon-dev libxkbcommon-dev libglib2.0-0 libgl1-mesa-dev python3-pip p7zip-full && \
    rm -rf /var/lib/{apt,dpkg,cache,log}

RUN pip3 install aqtinstall

WORKDIR /opt/qt

# =============================================================================
FROM base AS qt

ARG VERSION="7.0.0"
ARG QT_CREATOR_VERSION="7.0.0"
ARG QT_CREATOR_VERSION_MAJOR="7.0"
ARG QT_VERSION="6.2.3"
ARG QT_MODULES="-m qt5compat"

LABEL "org.opencontainers.image.title"="qtcreator-terminalplugin-dev"
LABEL "org.opencontainers.image.description"="A container for building the Qt Creator Terminal plugin for Linux desktop platform"
LABEL "org.opencontainers.image.version"="TerminalPlugin $VERSION, Qt Creator $QT_CREATOR_VERSION, Qt $QT_VERSION"
LABEL "org.opencontainers.image.source"="https://github.com/pa-sowa/QtCreatorTerminalPlugin"
LABEL "org.opencontainers.image.author"="Przemysław Adam Sowa <pa.sowa.dev@gmail.com>"

ENV QTDIR=/opt/qt/$QT_VERSION/gcc_64
ENV QTCREATOR_DIR=/opt/qtcreator

RUN aqt install-qt linux desktop $QT_VERSION $QT_MODULES && \
    rm /opt/qt/aqtinstall.log

ADD https://download.qt.io/official_releases/qtcreator/$QT_CREATOR_VERSION_MAJOR/$QT_CREATOR_VERSION/installer_source/linux_x64/qtcreator.7z /tmp/

ADD https://download.qt.io/official_releases/qtcreator/$QT_CREATOR_VERSION_MAJOR/$QT_CREATOR_VERSION/installer_source/linux_x64/qtcreator_dev.7z /tmp/

WORKDIR /opt/qtcreator

RUN 7z x /tmp/qtcreator.7z && rm /tmp/qtcreator.7z && \
    7z x /tmp/qtcreator_dev.7z && rm /tmp/qtcreator_dev.7z
