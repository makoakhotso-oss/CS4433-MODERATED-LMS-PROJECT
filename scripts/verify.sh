#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 2
ctest --test-dir "$BUILD_DIR" --output-on-failure
