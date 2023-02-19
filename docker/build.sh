#!/bin/bash
# This script builds docker images for building Qt Creator Terminal plugin for various versions of Qt Creator

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
VERSION=$1

if [[ "$VERSION" == "4.15.0" || -z "$VERSION" ]]; then
    # Qt Creator 4.15.x
    docker build -f "$SCRIPT_DIR/Dockerfile" --build-arg VERSION=4.15.0 --build-arg QT_CREATOR_VERSION=4.15.0 --build-arg QT_CREATOR_VERSION_MAJOR=4.15 --build-arg QT_VERSION=5.15.2 --build-arg QT_MODULES="" -t pasowa/qtcreator-terminal-plugin-dev:4.0 "$SCRIPT_DIR" \
        --label org.opencontainers.image.revision=$(git rev-parse --short HEAD) \
        --label org.opencontainers.image.created="$(date --rfc-3339=seconds --utc)"
fi

if [[ "$VERSION" == "5.0.0" || -z "$VERSION" ]]; then
    # Qt Creator 5.x
    docker build -f "$SCRIPT_DIR/Dockerfile" --build-arg VERSION=5.0.0 --build-arg QT_CREATOR_VERSION=5.0.0 --build-arg QT_CREATOR_VERSION_MAJOR=5.0 --build-arg QT_VERSION=5.15.2 --build-arg QT_MODULES="" -t pasowa/qtcreator-terminal-plugin-dev:5.0 "$SCRIPT_DIR" \
        --label org.opencontainers.image.revision=$(git rev-parse --short HEAD) \
        --label org.opencontainers.image.created="$(date --rfc-3339=seconds --utc)"
fi

if [[ "$VERSION" == "6.0.0" || -z "$VERSION" ]]; then
    # Qt Creator 6.x
    docker build -f "$SCRIPT_DIR/Dockerfile" --build-arg VERSION=6.0.0 --build-arg QT_CREATOR_VERSION=6.0.0 --build-arg QT_CREATOR_VERSION_MAJOR=6.0 --build-arg QT_VERSION=6.2.1 -t pasowa/qtcreator-terminal-plugin-dev:6.0 "$SCRIPT_DIR" \
        --label org.opencontainers.image.revision=$(git rev-parse --short HEAD) \
        --label org.opencontainers.image.created="$(date --rfc-3339=seconds --utc)"
fi

if [[ "$VERSION" == "7.0.0" || -z "$VERSION" ]]; then
    # Qt Creator 7.x
    docker build -f "$SCRIPT_DIR/Dockerfile" --build-arg VERSION=7.0.0 --build-arg QT_CREATOR_VERSION=7.0.0 --build-arg QT_CREATOR_VERSION_MAJOR=7.0 --build-arg QT_VERSION=6.2.3 -t pasowa/qtcreator-terminal-plugin-dev:7.0 "$SCRIPT_DIR" \
        --label org.opencontainers.image.revision=$(git rev-parse --short HEAD) \
        --label org.opencontainers.image.created="$(date --rfc-3339=seconds --utc)"
fi

if [[ "$VERSION" == "8.0.0" || -z "$VERSION" ]]; then
    # Qt Creator 8.x
    docker build -f "$SCRIPT_DIR/Dockerfile" --build-arg VERSION=8.0.0 --build-arg QT_CREATOR_VERSION=8.0.0 --build-arg QT_CREATOR_VERSION_MAJOR=8.0 --build-arg QT_VERSION=6.3.1 -t pasowa/qtcreator-terminal-plugin-dev:8.0 "$SCRIPT_DIR" \
        --label org.opencontainers.image.revision=$(git rev-parse --short HEAD) \
        --label org.opencontainers.image.created="$(date --rfc-3339=seconds --utc)"
fi

if [[ "$VERSION" == "9.0.0" || -z "$VERSION" ]]; then
    # Qt Creator 9.x
    docker build -f "$SCRIPT_DIR/Dockerfile" --build-arg VERSION=9.0.0 --build-arg QT_CREATOR_VERSION=9.0.0 --build-arg QT_CREATOR_VERSION_MAJOR=9.0 --build-arg QT_VERSION=6.4.0 -t pasowa/qtcreator-terminal-plugin-dev:9.0 "$SCRIPT_DIR" \
        --label org.opencontainers.image.revision=$(git rev-parse --short HEAD) \
        --label org.opencontainers.image.created="$(date --rfc-3339=seconds --utc)"
fi
