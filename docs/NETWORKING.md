# On-device Wi-Fi management

## Scope and ownership

The clock-in application does not replace OpenSTLinux networking. The existing
`wpa_supplicant@wlan0.service` owns radio authentication,
`udhcpc-wlan0.service` owns the IPv4 lease, and `systemd-resolved` remains the
resolver. `NetworkService` talks to wpa_supplicant through its root-only Unix
datagram control socket. This keeps one Clockin-owned systemd service and avoids
installing a second network manager.

The admin status separates three facts:

1. Wi-Fi association and current SSID.
2. A DHCP-provided IPv4 address on `wlan0`.
3. Reachability of the configured management API.

The third check deliberately uses the actual management server instead of a
public third-party connectivity endpoint.

## Supported networks

- Open networks (`key_mgmt=NONE`).
- WPA/WPA2 Personal (`WPA-PSK`).
- WPA3-SAE and WPA2/WPA3 transition networks (`SAE`/`WPA-PSK`).

Enterprise/EAP, IEEE 802.1X, WEP and OWE networks remain visible but disabled
with `Niet ondersteund`. Adding enterprise authentication would require a
separate identity/certificate design and is intentionally outside this UI.

Duplicate radios advertising one SSID are collapsed to the strongest result.
The list sorts the current network first, then saved networks, signal strength
and name. Selecting a row always opens details before any connection change.

## Connection transaction and rollback

New credentials first create a temporary `clockin_pending` profile. The worker
then:

1. records the previous network ID;
2. selects the temporary profile and waits up to 25 seconds for association;
3. restarts the existing DHCP unit and waits up to 20 seconds for IPv4;
4. marks the profile `clockin_saved` and calls `SAVE_CONFIG` only after both
   checks pass.

If association or DHCP fails, the temporary profile is removed and the previous
network is reselected. Stale pending profiles are removed on application start.
Management API failure does not roll Wi-Fi back when association and IPv4 are
healthy; it is shown as a separate server error.

`Verbinding verbreken` is session-only. `Netwerk vergeten` requires explicit
confirmation and persists removal. Forgetting the active/only network can end
SSH access, so acceptance testing must use a secondary hotspot and must never
forget the production `BitsEnBytes` profile.

## Credential handling

SSID values are sent to wpa_supplicant as hexadecimal bytes. Passphrases accept
8–63 printable characters or a 64-character hexadecimal raw PSK, are escaped as
one control value, and are never
placed in a shell command, process argument or journal message. Page, controller
and worker buffers are overwritten after use and on admin timeout.

Remembered credentials live only in
`/etc/wpa_supplicant/wpa_supplicant-wlan0.conf`, owned by root with mode `0600`.
WPA2 passphrases may be written by wpa_supplicant in recoverable form; the file
must therefore be treated as a secret and never copied into Git or support logs.

## One-time migration and read-only verification

Run while serial access is available if possible:

```bash
bash scripts/migrate-network-control.sh root@10.10.1.20
```

The script runs the board change in a transient systemd unit so it survives an
SSH interruption. It restores the original file automatically unless the
control socket responds, association returns, and `wlan0` receives IPv4.

Safe read-only diagnosis after migration:

```bash
ssh root@10.10.1.20 'wpa_cli -p /run/wpa_supplicant -i wlan0 ping'
ssh root@10.10.1.20 'wpa_cli -p /run/wpa_supplicant -i wlan0 status | grep -E "^(wpa_state|ssid|ip_address)="'
ssh root@10.10.1.20 'ip -4 address show dev wlan0'
```

Never print the full wpa_supplicant configuration. If a touchscreen network
test changes the address, read the new address on the admin screen and reconnect
SSH there. If rollback cannot restore access, use the DK2 serial console to
restart `wpa_supplicant@wlan0.service`, inspect only non-secret status fields,
and restore a known root-only configuration.
