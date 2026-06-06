#!/bin/bash
# setup_sdk.sh - Download and install STM32MP1 OpenSTLinux Weston SDK
# Prerequisites: ST.com account (register at https://www.st.com)

set -e

SDK_VERSION="openstlinux-6.6-yocto-scarthgap-mpu-v26.02.18"
SDK_TARBALL="SDK-x86_64-stm32mp1-${SDK_VERSION}.tar.gz"
SDK_DOWNLOAD_URL="https://www.st.com/en/embedded-software/stm32mp1dev.html#get-software"
SDK_INSTALL_DIR="/opt/st/stm32mp1"

echo "=============================================="
echo " STM32MP1 OpenSTLinux SDK Setup"
echo " Target: STM32MP157DK-2 (Cortex-A7, scarthgap)"
echo "=============================================="
echo ""

# Check if already installed
if [ -d "$SDK_INSTALL_DIR" ] && ls "$SDK_INSTALL_DIR"/*/environment-setup-* 1>/dev/null 2>&1; then
    echo "SDK appears to be already installed in $SDK_INSTALL_DIR"
    ENV_SCRIPT=$(find "$SDK_INSTALL_DIR" -name "environment-setup-*" 2>/dev/null | head -1)
    echo "Environment script: $ENV_SCRIPT"
    echo ""
    echo "To use the SDK:"
    echo "  source $ENV_SCRIPT"
    exit 0
fi

# Check if tarball exists in common locations
TARBALL_PATH=""
for dir in /mnt/c/Users/*/Downloads "$HOME" "$(pwd)"; do
    if [ -f "${dir}/${SDK_TARBALL}" ]; then
        TARBALL_PATH="${dir}/${SDK_TARBALL}"
        break
    fi
done

if [ -z "$TARBALL_PATH" ]; then
    echo "SDK tarball not found locally."
    echo ""
    echo "Please download it from:"
    echo "  ${SDK_DOWNLOAD_URL}"
    echo ""
    echo "1. Open the URL in a browser"
    echo "2. Click 'Get Software' (requires ST.com login - free registration)"
    echo "3. Download: ${SDK_TARBALL}"
    echo "4. Place the file in your Windows Downloads folder:"
    echo "   C:\\Users\\<your-user>\\Downloads\\"
    echo ""
    echo "Or in this directory: $(pwd)"
    echo ""
    echo "Then re-run this script."
    exit 1
fi

echo "Found SDK tarball: $TARBALL_PATH"

# Extract the SDK tarball
WORK_DIR="/tmp/st-sdk-setup-$$"
mkdir -p "$WORK_DIR"
echo "Extracting SDK tarball..."
tar xzf "$TARBALL_PATH" -C "$WORK_DIR"

# Find the SDK installer script
SDK_INSTALLER=$(find "$WORK_DIR" -name "st-image-weston-*-x86_64-toolchain-*.sh" | head -1)

if [ -z "$SDK_INSTALLER" ]; then
    echo "ERROR: Cannot find SDK installer .sh script in extracted tarball"
    echo "Contents of $WORK_DIR:"
    find "$WORK_DIR" -type f | head -20
    rm -rf "$WORK_DIR"
    exit 1
fi

echo "Found SDK installer: $(basename "$SDK_INSTALLER")"

# Install the SDK
echo "Installing SDK to $SDK_INSTALL_DIR..."
sudo mkdir -p "$SDK_INSTALL_DIR"
sudo chmod 755 "$SDK_INSTALL_DIR"

chmod +x "$SDK_INSTALLER"
"$SDK_INSTALLER" -d "$SDK_INSTALL_DIR" -y

# Cleanup
rm -rf "$WORK_DIR"

echo ""
echo "=============================================="
echo " SDK installation complete!"
echo "=============================================="
echo ""

# Find and source environment
ENV_SCRIPT=$(find "$SDK_INSTALL_DIR" -name "environment-setup-*" 2>/dev/null | head -1)
echo "To use the SDK in a new terminal:"
echo "  source $ENV_SCRIPT"
echo ""
echo "To build the project:"
echo "  cd $(dirname "$0") && ./build.sh"
