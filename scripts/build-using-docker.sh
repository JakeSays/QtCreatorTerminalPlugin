#!/bin/bash

VERSION=$1

if [[ -z "$VERSION" ]]; then
    echo "Usage: $0 <Qt Creator major.minor version>"
    echo "ie. $0 9.0"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_DIR="$SCRIPT_DIR/.."
OUT_DIR="$PWD/build-$VERSION"

set -e
mkdir -p "$OUT_DIR"
docker run -it --rm -v "$REPO_DIR":/workspace:ro -v "$OUT_DIR":/tmp/install pasowa/qtcreator-terminal-plugin-dev:$VERSION /workspace/scripts/build.sh
