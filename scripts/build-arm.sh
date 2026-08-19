#!/usr/bin/env bash
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${CLOCKIN_ARM_BUILD_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/clockin-device/arm}"
sdk_environment="${STM32_SDK_ENV:-}"

if [[ -z "$sdk_environment" ]]; then
    sdk_environment="$(find /opt/st/stm32mp1 -type f -name 'environment-setup-*' \
        -print 2>/dev/null | sort | tail -n 1 || true)"
fi
if [[ -z "$sdk_environment" || ! -f "$sdk_environment" ]]; then
    echo "OpenSTLinux STM32MP1 SDK environment not found." >&2
    echo "Install it below /opt/st/stm32mp1 or set STM32_SDK_ENV." >&2
    exit 1
fi

# The SDK intentionally exports its compiler, sysroot and CMake settings.
# shellcheck disable=SC1090
source "$sdk_environment"
cmake -S "$source_root" -B "$build_root" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCLOCKIN_BUILD_APP=ON \
    -DCLOCKIN_BUILD_DIAGNOSTIC=ON \
    -DCLOCKIN_BUILD_TESTS=OFF
cmake --build "$build_root" --parallel

file "$build_root/clockin-device" "$build_root/clockin-hardware-diagnostic"
if strings "$build_root/clockin-device" | \
    grep -Eq 'DESKTOP_SIM|STM32_SIM_|ttyRPMSG|green_on|green_off|red_on|red_off'; then
    echo "Forbidden legacy or simulator string found in production binary." >&2
    exit 1
fi
echo "ARM output: $build_root"
