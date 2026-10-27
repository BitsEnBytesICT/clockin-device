#!/usr/bin/env bash
# Cross-builds the A7 binaries with Ubuntu 24.04's armhf toolchain, for hosts
# without the OpenSTLinux SDK. Ubuntu noble and OpenSTLinux 5.0.8 (scarthgap)
# share glibc 2.39 and GCC 13, so only the board-specific libraries differ:
# curl, libgpiod v2 and the Vivante EGL/GLES are linked from copies taken off
# the board itself, never from Ubuntu's armhf packages.
#
# One-time host setup (as root in WSL):
#   dpkg --add-architecture armhf   # plus an armhf ports.ubuntu.com source
#   apt-get install cmake build-essential file pkg-config g++-arm-linux-gnueabihf \
#     libwayland-bin libcurl4-openssl-dev:armhf libegl-dev:armhf \
#     libgles-dev:armhf libwayland-dev:armhf libxkbcommon-dev:armhf
#
# Usage: bash scripts/build-arm-ubuntu.sh [root@10.10.1.20]
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
board="${1:-root@10.10.1.20}"
cache_root="${XDG_CACHE_HOME:-$HOME/.cache}/clockin-device"
build_root="${CLOCKIN_ARM_BUILD_DIR:-$cache_root/arm}"
board_libs="${CLOCKIN_BOARD_LIBS:-$cache_root/board-libs}"
gpiod_version="v2.1.1"

if [[ ! -f "$board_libs/libgpiod.so.3.1.1" ]]; then
    echo "Copying link libraries from $board into $board_libs"
    mkdir -p "$board_libs"
    scp -q "$board:/usr/lib/libcurl.so.4.8.0" "$board:/usr/lib/libgpiod.so.3.1.1" \
        "$board:/vendor/lib/libEGL.so.1.5.0" "$board:/vendor/lib/libGLESv2.so.2.0.0" \
        "$board_libs/"
fi
for library in libcurl.so.4.8.0:libcurl.so.4 libgpiod.so.3.1.1:libgpiod.so.3 \
               libEGL.so.1.5.0:libEGL.so.1 libGLESv2.so.2.0.0:libGLESv2.so.2; do
    file_name="${library%%:*}"
    soname="${library##*:}"
    [[ -f "$board_libs/$file_name" ]] || { echo "Missing $board_libs/$file_name" >&2; exit 1; }
    ln -sf "$file_name" "$board_libs/$soname"
done
if [[ ! -f "$board_libs/include/gpiod.h" ]]; then
    mkdir -p "$board_libs/include"
    curl -fsSL -o "$board_libs/include/gpiod.h" \
        "https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/plain/include/gpiod.h?h=$gpiod_version"
fi

toolchain="$build_root/ubuntu-armhf.cmake"
mkdir -p "$build_root"
cat > "$toolchain" <<'TOOLCHAIN'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
# The board's libraries reference their own dependencies (OpenSSL, galcore...),
# which are resolved on the device at run time.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--allow-shlib-undefined")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
TOOLCHAIN

export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig
cmake -S "$source_root" -B "$build_root" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-I$board_libs/include" \
    -DCLOCKIN_BUILD_APP=ON \
    -DCLOCKIN_BUILD_DIAGNOSTIC=ON \
    -DCLOCKIN_BUILD_TESTS=OFF \
    -DCURL_INCLUDE_DIR=/usr/include/arm-linux-gnueabihf \
    -DCURL_LIBRARY="$board_libs/libcurl.so.4" \
    -DGPIOD_LIBRARY="$board_libs/libgpiod.so.3" \
    -DEGL_LIBRARY="$board_libs/libEGL.so.1" \
    -DGLES2_LIBRARY="$board_libs/libGLESv2.so.2"
cmake --build "$build_root" --parallel

file "$build_root/clockin-device" "$build_root/clockin-hardware-diagnostic"
if strings "$build_root/clockin-device" | \
    grep -Eq 'DESKTOP_SIM|STM32_SIM_|ttyRPMSG|green_on|green_off|red_on|red_off'; then
    echo "Forbidden legacy or simulator string found in production binary." >&2
    exit 1
fi
echo "ARM output: $build_root"
