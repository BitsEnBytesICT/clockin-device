#!/bin/sh
set -eu

config=/etc/wpa_supplicant/wpa_supplicant-wlan0.conf
marker=/run/clockin-network-migration.ok
backup=
temporary=
installed=0

rollback() {
    if [ "$installed" -eq 1 ] && [ -n "$backup" ] && [ -f "$backup" ]; then
        install -o root -g root -m 0600 "$backup" "$config"
        systemctl restart wpa_supplicant@wlan0.service || true
        systemctl restart udhcpc-wlan0.service || true
    fi
    [ -z "$backup" ] || rm -f "$backup"
    [ -z "$temporary" ] || rm -f "$temporary"
    rm -f "$marker"
}

trap 'rollback' EXIT HUP INT TERM

[ "$(id -u)" -eq 0 ] || { echo "This migration must run as root." >&2; exit 1; }
[ -s "$config" ] || { echo "The wlan0 wpa_supplicant configuration is missing." >&2; exit 1; }

backup=$(mktemp /etc/wpa_supplicant/.wlan0-clockin-backup.XXXXXX)
chmod 0600 "$backup"
cp -p "$config" "$backup"

temporary=$(mktemp /etc/wpa_supplicant/.wlan0-clockin-new.XXXXXX)
chmod 0600 "$temporary"
awk '
    BEGIN { control = 0; update = 0 }
    /^[[:space:]]*ctrl_interface[[:space:]]*=/ {
        if (!control) print "ctrl_interface=DIR=/run/wpa_supplicant GROUP=root"
        control = 1
        next
    }
    /^[[:space:]]*update_config[[:space:]]*=/ {
        if (!update) print "update_config=1"
        update = 1
        next
    }
    {
        lower = tolower($0)
        if (lower ~ /^[[:space:]]*#[[:space:]]*(psk|password|sae_password)[[:space:]]*=/) next
        print
    }
    END {
        if (!control) print "ctrl_interface=DIR=/run/wpa_supplicant GROUP=root"
        if (!update) print "update_config=1"
    }
' "$config" > "$temporary"
chown root:root "$temporary"
chmod 0600 "$temporary"
mv "$temporary" "$config"
temporary=
installed=1

systemctl restart wpa_supplicant@wlan0.service
systemctl restart udhcpc-wlan0.service

attempt=0
while [ "$attempt" -lt 45 ]; do
    if [ -S /run/wpa_supplicant/wlan0 ] &&
       [ "$(wpa_cli -p /run/wpa_supplicant -i wlan0 ping 2>/dev/null || true)" = PONG ] &&
       wpa_cli -p /run/wpa_supplicant -i wlan0 status 2>/dev/null |
           grep -q '^wpa_state=COMPLETED$' &&
       ip -4 -o address show dev wlan0 scope global | grep -q 'inet '; then
        touch "$marker"
        chmod 0600 "$marker"
        installed=0
        rm -f "$backup"
        backup=
        trap - EXIT HUP INT TERM
        exit 0
    fi
    attempt=$((attempt + 1))
    sleep 1
done

echo "Wi-Fi control migration did not recover association and IPv4; rolling back." >&2
exit 1
