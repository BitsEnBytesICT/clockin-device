#!/usr/bin/env bash
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${CLOCKIN_HOST_BUILD_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/clockin-device/host}"

cmake -S "$source_root" -B "$build_root" -G "Unix Makefiles" \
    -DCLOCKIN_BUILD_APP=OFF \
    -DCLOCKIN_BUILD_DIAGNOSTIC=OFF \
    -DCLOCKIN_BUILD_TESTS=ON \
    -DCLOCKIN_ENABLE_SANITIZERS=ON
cmake --build "$build_root" --parallel
ASAN_OPTIONS=detect_leaks=1 \
    ctest --test-dir "$build_root" --output-on-failure
