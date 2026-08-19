#!/usr/bin/env bash
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${CLOCKIN_ARM_BUILD_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/clockin-device/arm}"
board="${CLOCKIN_BOARD:-${1:-root@192.168.11.51}}"
stock_dtb="$source_root/board/device-tree/build/stm32mp157f-dk2-stock.dtb"
clockin_dtb="$source_root/board/device-tree/build/stm32mp157f-dk2-clockin.dtb"
boot_config="$source_root/board/device-tree/extlinux-clockin.conf"
diagnostic="$build_root/clockin-hardware-diagnostic"

for required in "$stock_dtb" "$clockin_dtb" "$boot_config" "$diagnostic"; do
    [[ -f "$required" ]] || { echo "Missing deployment input: $required" >&2; exit 1; }
done

ssh "$board" 'set -eu
    install -d -m 0700 /boot/clockin-backup
    test -f /boot/clockin-backup/stm32mp157f-dk2.dtb || cp -p /boot/stm32mp157f-dk2.dtb /boot/clockin-backup/
    test -f /boot/clockin-backup/extlinux.conf || cp -p /boot/mmc0_extlinux/extlinux.conf /boot/clockin-backup/
    test -f /boot/clockin-backup/stm32mp157f-dk2_extlinux.conf || cp -p /boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf /boot/clockin-backup/'
scp "$clockin_dtb" "$board:/boot/stm32mp157f-dk2-clockin.dtb.new"
scp "$boot_config" "$board:/boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf.new"
scp "$diagnostic" "$board:/usr/local/bin/clockin-hardware-diagnostic.new"
ssh "$board" 'set -eu
    mv /boot/stm32mp157f-dk2-clockin.dtb.new /boot/stm32mp157f-dk2-clockin.dtb
    mv /boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf.new /boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf
    install -m 0755 /usr/local/bin/clockin-hardware-diagnostic.new /usr/local/bin/clockin-hardware-diagnostic
    rm -f /usr/local/bin/clockin-hardware-diagnostic.new
    chmod 0644 /boot/stm32mp157f-dk2-clockin.dtb /boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf
    sync
    echo "A7 boot resources installed; reboot when serial recovery is available."'
