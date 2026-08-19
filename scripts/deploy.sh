#!/usr/bin/env bash
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${CLOCKIN_ARM_BUILD_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/clockin-device/arm}"
board="${CLOCKIN_BOARD:-${1:-root@192.168.11.51}}"
binary="$build_root/clockin-device"
diagnostic="$build_root/clockin-hardware-diagnostic"
unit="$source_root/board/systemd/clockin-device.service"
cleanup="$source_root/board/systemd/disable-buzzer.sh"

for required in "$binary" "$diagnostic" "$unit" "$cleanup"; do
    [[ -f "$required" ]] || { echo "Missing deployment input: $required" >&2; exit 1; }
done

validate_single_line() {
    local name="$1" value="$2"
    [[ "$value" != *$'\n'* && "$value" != *$'\r'* ]] || {
        echo "$name must contain exactly one line." >&2
        exit 1
    }
}

install_secret() {
    local value="$1" destination="$2"
    printf '%s\n' "$value" | ssh "$board" \
        "umask 077; cat > '$destination.new'; chown root:root '$destination.new'; chmod 0600 '$destination.new'; mv '$destination.new' '$destination'"
}

ssh "$board" 'set -eu
    grep -qw systemd.mask=m4-autostart.service /proc/cmdline
    ls /dev/spidev* >/dev/null
    ls /sys/class/pwm/pwmchip* >/dev/null
    install -d -o root -g root -m 0700 /etc/clockin-device
    install -d -o root -g root -m 0755 /usr/local/lib/clockin-device/backup'

if [[ -n "${CLOCKIN_API_BASE_URL:-}" ]]; then
    validate_single_line CLOCKIN_API_BASE_URL "$CLOCKIN_API_BASE_URL"
    printf 'BITS_BYTES_API_BASE_URL=%s\nBITS_BYTES_API_KEY_FILE=/etc/clockin-device/rfid-api-key\nBITS_BYTES_ADMIN_PIN_FILE=/etc/clockin-device/admin-pin\nBITS_BYTES_TOUCH_DEVICE=/dev/input/event1\nBITS_BYTES_FRAMEBUFFER_FORMAT=auto\n' \
        "$CLOCKIN_API_BASE_URL" | ssh "$board" \
        'umask 077; cat > /etc/clockin-device/environment.new; chown root:root /etc/clockin-device/environment.new; chmod 0600 /etc/clockin-device/environment.new; mv /etc/clockin-device/environment.new /etc/clockin-device/environment'
elif ! ssh "$board" 'test -s /etc/clockin-device/environment'; then
    echo "Set CLOCKIN_API_BASE_URL for the first deployment." >&2
    exit 1
fi

if [[ -n "${CLOCKIN_API_KEY:-}" ]]; then
    validate_single_line CLOCKIN_API_KEY "$CLOCKIN_API_KEY"
    install_secret "$CLOCKIN_API_KEY" /etc/clockin-device/rfid-api-key
elif ! ssh "$board" 'test -s /etc/clockin-device/rfid-api-key'; then
    echo "Set CLOCKIN_API_KEY for the first deployment." >&2
    exit 1
fi

if [[ -n "${CLOCKIN_ADMIN_PIN:-}" ]]; then
    [[ "$CLOCKIN_ADMIN_PIN" =~ ^[0-9]{4}$ ]] || {
        echo "CLOCKIN_ADMIN_PIN must contain exactly four digits." >&2
        exit 1
    }
    install_secret "$CLOCKIN_ADMIN_PIN" /etc/clockin-device/admin-pin
elif ! ssh "$board" 'test -s /etc/clockin-device/admin-pin'; then
    echo "Set CLOCKIN_ADMIN_PIN for the first deployment." >&2
    exit 1
fi

scp "$binary" "$board:/tmp/clockin-device.new"
scp "$diagnostic" "$board:/tmp/clockin-hardware-diagnostic.new"
scp "$unit" "$board:/tmp/clockin-device.service.new"
scp "$cleanup" "$board:/tmp/disable-buzzer.sh.new"

ssh "$board" 'set -eu
    if test -e /usr/local/bin/clockin-device; then
        cp -p /usr/local/bin/clockin-device /usr/local/lib/clockin-device/backup/clockin-device.previous
    fi
    if test -e /etc/systemd/system/clockin-device.service; then
        cp -p /etc/systemd/system/clockin-device.service /usr/local/lib/clockin-device/backup/clockin-device.service.previous
    fi
    systemctl stop clockin-device.service 2>/dev/null || true
    install -o root -g root -m 0755 /tmp/clockin-device.new /usr/local/bin/clockin-device
    install -o root -g root -m 0755 /tmp/clockin-hardware-diagnostic.new /usr/local/bin/clockin-hardware-diagnostic
    install -o root -g root -m 0755 /tmp/disable-buzzer.sh.new /usr/local/lib/clockin-device/disable-buzzer.sh
    install -o root -g root -m 0644 /tmp/clockin-device.service.new /etc/systemd/system/clockin-device.service
    chown root:root /etc/clockin-device/environment /etc/clockin-device/rfid-api-key /etc/clockin-device/admin-pin
    chmod 0600 /etc/clockin-device/environment /etc/clockin-device/rfid-api-key /etc/clockin-device/admin-pin
    systemctl daemon-reload
    systemd-analyze verify /etc/systemd/system/clockin-device.service
    systemctl disable --now imgui-app.service 2>/dev/null || true
    systemctl disable --now m4-autostart.service 2>/dev/null || true
    systemctl enable --now clockin-device.service
    systemctl is-active --quiet clockin-device.service
    rm -f /tmp/clockin-device.new /tmp/clockin-hardware-diagnostic.new /tmp/clockin-device.service.new /tmp/disable-buzzer.sh.new
    sync
    systemctl show clockin-device.service -p ActiveState -p SubState -p MainPID -p NRestarts -p Result
    journalctl -u clockin-device.service --since "-2 min" --no-pager -n 40'
