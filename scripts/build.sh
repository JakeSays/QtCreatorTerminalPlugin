#!/bin/bash

# This script builds and installs the plugin
# It is meant to be run inside pasowa/qtcreator-terminal-plugin-dev docker container
# where the git repository is available at /workspace and /tmp/install is mounted to export the result to the host.

# exit when any command fails
set -e

cd /tmp
cmake -DCMAKE_PREFIX_PATH="$QTDIR;$QTCREATOR_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/install /workspace
cmake --build . -j$(nproc)
cmake --install .
