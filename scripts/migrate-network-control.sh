#!/usr/bin/env bash
set -euo pipefail

source_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
board="${CLOCKIN_BOARD:-${1:-root@10.10.1.20}}"
script="$source_root/board/network/enable-wpa-control.sh"

[[ -f "$script" ]] || { echo "Missing migration script: $script" >&2; exit 1; }
scp "$script" "$board:/tmp/enable-wpa-control.sh"
ssh "$board" 'rm -f /run/clockin-network-migration.ok
    chmod 0700 /tmp/enable-wpa-control.sh
    systemctl stop clockin-network-control-migration.service 2>/dev/null || true
    systemd-run --unit=clockin-network-control-migration --collect \
        /bin/sh /tmp/enable-wpa-control.sh'

ready=0
for _ in $(seq 1 40); do
    if ssh -o ConnectTimeout=3 "$board" \
        'test -f /run/clockin-network-migration.ok' 2>/dev/null; then
        ready=1
        break
    fi
    sleep 3
done

if [[ "$ready" -ne 1 ]]; then
    echo "Migration did not report success. The board script automatically rolls back." >&2
    echo "Use serial recovery if the original network did not return." >&2
    exit 1
fi

ssh "$board" 'set -eu
    test "$(stat -c %a /etc/wpa_supplicant/wpa_supplicant-wlan0.conf)" = 600
    test "$(wpa_cli -p /run/wpa_supplicant -i wlan0 ping)" = PONG
    wpa_cli -p /run/wpa_supplicant -i wlan0 status | grep -E "^(wpa_state|ssid|ip_address)="
    wpa_cli -p /run/wpa_supplicant -i wlan0 scan >/dev/null
    sleep 2
    count=$(wpa_cli -p /run/wpa_supplicant -i wlan0 scan_results | tail -n +2 | wc -l)
    echo "Visible Wi-Fi networks: $count"
    rm -f /tmp/enable-wpa-control.sh /run/clockin-network-migration.ok'

echo "wpa_supplicant control access is enabled and the existing connection recovered."
