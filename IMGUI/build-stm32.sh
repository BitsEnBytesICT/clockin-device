#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sdk_environment="${STM32_SDK_ENV:-}"

if [[ -z "$sdk_environment" ]]; then
    sdk_environment="$(find /opt/st/stm32mp1 -type f -name 'environment-setup-*' -print 2>/dev/null | sort | tail -n 1 || true)"
fi

if [[ -z "$sdk_environment" || ! -f "$sdk_environment" ]]; then
    echo "STM32MP1 SDK environment not found." >&2
    echo "Install the OpenSTLinux SDK under /opt/st/stm32mp1 or set STM32_SDK_ENV." >&2
    exit 1
fi

echo "+ Using SDK environment: $sdk_environment"
# The ST SDK intentionally exports compiler, sysroot and CMake variables.
# shellcheck disable=SC1090
source "$sdk_environment"

cmake -S "$project_dir" -B "$project_dir/build-stm32" \
    -DDESKTOP_SIM=OFF \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_dir/build-stm32" --parallel

file "$project_dir/build-stm32/imgui_app"
