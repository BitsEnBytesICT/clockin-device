# Test and acceptance record

## Automated host coverage

`scripts/test-host.sh` compiles hand-written test code with
`-Wall -Wextra -Wpedantic`, AddressSanitizer and UndefinedBehaviorSanitizer. The
tests do not initialize GLFW, EGL or a desktop window. They cover:

- all navigation destinations and transition timestamps;
- held-card deduplication, removal timeout and fresh presentation;
- clock-in, attendance, signature and clock-out workflow results;
- rejection, cancellation, transport timeout and uncertain API outcomes;
- signature minimums, simplification, point and payload bounds;
- Linux multitouch report parsing and 800×480 rotation;
- MFRC522 register-read SPI framing with a fake transport;
- semantic buzzer cue pitch, duty cycle, timing and final disabled state;
- configuration validation and missing-secret behavior;
- Wi-Fi scan parsing, duplicate SSID selection, security classification and safe encoding;
- atomic mode-`0600` administrator PIN replacement;
- admin tab routing, double PIN confirmation and touch-keyboard editing;
- nested, escaped and malformed JSON.

## Automated production checks

`scripts/build-arm.sh` cross-compiles both binaries with the OpenSTLinux 5.0.8
SDK; `scripts/build-arm-ubuntu.sh` does the same with Ubuntu 24.04's armhf
toolchain. Both check that they are ARM executables and reject simulator, RPMsg
and LED-command strings. After an Ubuntu-toolchain build, confirm every library
resolves on the board before deploying:

```bash
scp ~/.cache/clockin-device/arm/clockin-device root@10.10.1.20:/tmp/clockin-device.check
ssh root@10.10.1.20 'chmod 700 /tmp/clockin-device.check; LD_TRACE_LOADED_OBJECTS=1 /tmp/clockin-device.check | grep "not found"; rm /tmp/clockin-device.check'
```

No output from the `grep` means nothing is missing. Credential values must also be checked before release with
a targeted `strings` scan using the values held outside the repository.

## Board acceptance checklist

Do not delete the quarantined pre-refactor WSL tree or stock M4 recovery path
until every item below is complete:

- The service starts with Weston, stays active and does not log credentials.
- MFRC522 version access succeeds and ten card presentations return one UID.
- Holding a card produces one workflow; removal plus presentation produces one new workflow.
- The user confirms card, success, warning and error cues are audible but not
  intrusive.
- PWM is disabled after sound, process stop and abnormal service restart.
- Clock-in, attendance, signature, clock-out, admin PIN and error screens work.
- RFID, PIN, Network and API admin tabs animate and remain touch responsive.
- Changing the API address on the API tab survives a restart, and an invalid
  address is rejected without leaving the keyboard.
- `systemctl restart clockin-device.service` completes in well under 5 s and
  the journal shows `Shutdown complete in ... ms`.
- After 60 s without activity the backlight turns off and the app stops
  rendering; a touch or card wakes it immediately.
- The admin shell returns to Waiting after 120 seconds without touch activity.
- A changed PIN survives an application restart and the old PIN no longer unlocks.
- A wrong Wi-Fi password restores the previous association and IPv4 address.
- A second WPA2/WPA3 or open test network connects and displays the new address.
- Returning to a saved network does not request its password again.
- Disconnect is session-only; Forget removes only the explicitly confirmed test network.
- The production `BitsEnBytes` profile is never used for a Forget test.
- The management-server indicator changes independently from Wi-Fi and IPv4 status.
- Touchscreen corners, attendance scrolling and signature drawing are correct.
- Backlight dims after 30 seconds, turns off after 60 seconds and wakes on activity.
- Disconnecting the RFID reader does not freeze rendering or touch.
- Three consecutive reboots start `clockin-device.service` successfully.
- The device runs for at least one hour without service restart or growing errors.
- The serial `stock-m4` selection still restores the original recovery path.

Record the service status, boot ID and journal result for each reboot. Physical
card, touchscreen and audible checks require a person at the board; compilation
and SSH logs cannot prove them.
