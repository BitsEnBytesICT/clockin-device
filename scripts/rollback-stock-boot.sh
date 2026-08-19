#!/usr/bin/env bash
set -euo pipefail

board="${CLOCKIN_BOARD:-${1:-root@192.168.11.51}}"
ssh "$board" 'set -eu
    test -f /boot/clockin-backup/extlinux.conf
    test -f /boot/clockin-backup/stm32mp157f-dk2_extlinux.conf
    cp -p /boot/clockin-backup/extlinux.conf /boot/mmc0_extlinux/extlinux.conf
    cp -p /boot/clockin-backup/stm32mp157f-dk2_extlinux.conf /boot/mmc0_extlinux/stm32mp157f-dk2_extlinux.conf
    sync
    echo "Stock boot configuration restored; reboot to return to the M4 recovery path."'
