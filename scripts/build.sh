#!/bin/bash

cd /tmp && \
cmake -DCMAKE_PREFIX_PATH="$QTDIR;$QTCREATOR_DIR" -DCMAKE_BUILD_TYPE=Release /workspace && \
cmake --build . -j$(nproc)

