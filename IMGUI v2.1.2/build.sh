#!/bin/bash
# build.sh - Cross-compile RFID Attendance System for STM32MP157DK-2
# Usage: ./build.sh [SDK_DIR]
#   SDK_DIR: path to installed SDK (default: auto-detect under /opt/st/)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Find SDK environment setup script
if [ -n "$1" ]; then
    SDK_DIR="$1"
else
    SDK_DIR=$(ls -dt /opt/st/stm32mp1/*/ 2>/dev/null | head -1)
fi

if [ -z "$SDK_DIR" ]; then
    echo "ERROR: STM32MP1 SDK not found in /opt/st/stm32mp1/"
    echo "Download from: https://www.st.com/en/embedded-software/stm32mp1dev.html#get-software"
    echo "Then run: ./setup_sdk.sh"
    exit 1
fi

ENV_SCRIPT=$(find "$SDK_DIR" -maxdepth 1 -name "environment-setup-*" 2>/dev/null | head -1)

if [ -z "$ENV_SCRIPT" ]; then
    echo "ERROR: Cannot find environment-setup script in $SDK_DIR"
    exit 1
fi

echo "Using SDK: $SDK_DIR"
echo "Sourcing: $ENV_SCRIPT"
source "$ENV_SCRIPT"

echo ""
echo "=== Configuring project ==="
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo ""
echo "=== Building project ==="
cmake --build "$BUILD_DIR" -j "$(nproc)"

echo ""
echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/rfid-attendance"
file "${BUILD_DIR}/rfid-attendance"
ls -lh "${BUILD_DIR}/rfid-attendance"

echo ""
echo "To deploy to the board:"
echo "  scp build/rfid-attendance root@<board-ip>:/usr/local/bin/"
