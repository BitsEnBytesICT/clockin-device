# Admin and network validation — 2026-08-26

## Automated results

- Headless host suite: 34 tests, 0 failures, with AddressSanitizer and
  UndefinedBehaviorSanitizer enabled.
- Production build: OpenSTLinux 5.0.8 STM32MP1 SDK, ARM EABI5 executable.
- Deployed binary SHA-256:
  `4e50bb48586f9b3d7755208ba073614db864966be00685178f626cd56515d673`.
- The build and `/usr/local/bin/clockin-device` hashes match.
- The deployed binary contains neither the configured API key nor admin PIN.
- Shell syntax checks pass for deployment and reversible network migration.

## Board results

- One-time wpa_supplicant migration completed and removed its temporary backup.
- The control socket responds and the application owns exactly one local client
  socket below its mode-`0700` systemd runtime directory.
- Existing `BitsEnBytes` association recovered with IPv4 `10.10.1.20`.
- The wpa_supplicant configuration remains root-owned and mode `0600`.
- `clockin-device.service`, `wpa_supplicant@wlan0.service` and
  `udhcpc-wlan0.service` are active.
- The kiosk reports a 50 Hz RGB565 display, touchscreen, MFRC522 version `0x92`,
  and a reachable production management API.
- The semantic buzzer diagnostic completed the card, success, warning and error
  cues and left PWM disabled.
- A deliberate SIGKILL changed the application PID and systemd restarted it in
  five seconds. RFID, touchscreen, API and the WPA control client all reopened.
  A later clean final deployment reset the current unit counter to
  `NRestarts=0`; the crash/restart evidence remains in that boot's journal.
- No Wi-Fi password, PSK, private key or API-key marker appeared in the
  application journal.

## Deliberately pending physical acceptance

No touchscreen-driven association, disconnect or forget command was issued over
SSH. A person at the device must still verify:

1. Appearance, animation and touch regions of all three admin tabs.
2. PIN mismatch/retry and persistence using a temporary known PIN.
3. Wrong-password rollback using a secondary hotspot.
4. Successful connection to that hotspot and display of its new IP address.
5. Return to `BitsEnBytes` without re-entering its password.
6. Disconnect/reconnect and Forget only for the secondary test network.
7. RFID UID display remains responsive throughout.
8. Card, success, warning and error cues are audible without being intrusive.

Do not push this work to GitHub until those physical checks pass. Never use the
production `BitsEnBytes` profile for the Forget test.
