# Validation record — 2026-08-19

## Completed automatically

- Host unit suite compiled with GCC 13, `-Wall -Wextra -Wpedantic`, ASan and UBSan.
- All 18 headless tests passed.
- Application and diagnostic cross-compiled with the installed OpenSTLinux 5.0.8 SDK.
- Both outputs are 32-bit ARM EABI5 Linux executables.
- Production string scan found no simulator, RPMsg or LED-command markers.
- Production string scan found no migrated API key or administrator PIN.
- Local and installed SHA-256 values match:

| File | SHA-256 |
|---|---|
| `clockin-device` | `3b1a8a0f9bcbcdabf87d3f7d727b02aa43b72331b4630fb22ff4e155f3ddcc92` |
| `clockin-hardware-diagnostic` | `6d9b9bd86ad312b81d4ad367067da7a5981988a3f0b6a0c4c97563320a13cc23` |

- Board configuration directory is root-owned mode `0700`; environment, API key and PIN files are root-owned mode `0600`.
- Journal scan found no credential values.
- Display initialized as RGB565 (`R5 G6 B5 A0`) at 50 Hz.
- Touch initialized at `/dev/input/event1`.
- MFRC522 initialized on `/dev/spidev0.0`, version register `0x92`.
- Diagnostic emitted the single/triple buzzer patterns and left PWM disabled.
- `imgui-app.service` is disabled and inactive.
- `m4-autostart.service` is masked and inactive.
- The kiosk unit passed `systemd-analyze verify` on the board.
- `clockin-device.service` is enabled, active and configured with `Restart=always`.
- A forced `SIGKILL` changed the managed PID from `3768` to `3795` after the
  configured five-second delay and increased `NRestarts` from zero to one.
- Clean service stop completed in 0.87 seconds and logged both shutdown stages.
- A stop while the API connection was in progress also completed cleanly; the PWM channel was unexported.

## Reboot evidence

Three consecutive boots reached the A7 application with hardware initialized and
the service active with zero restarts:

1. `6006fe4d-53e4-4258-81a7-b41efa413c9a`
2. `c63b2dbb-9b85-4dd6-8918-27bc12690270`
3. `9450d1e2-c4d7-49da-a4da-ccb437b86735`
4. `08029efb-5476-4d7e-aabb-16496134df34` — kiosk update validation;
   Weston and the application were active, the service had zero boot-time
   restarts, and display, touch, API and MFRC522 initialization completed.

## Pending manual/connected checks

The configured API at `http://192.168.11.194:3000` was reachable during the
latest kiosk deployment and reboot. Full clock-in/out, attendance and signed
workflow checks were not exercised during this service-only change. A person at
the device must also confirm:

- ten repeat UID presentations and held-card behavior;
- all touchscreen corners, scrolling and signature drawing;
- visible admin, success and error screens;
- backlight dim/off/wake timings;
- behavior while the RFID reader is physically disconnected;
- audible quality of the newly deployed buzzer patterns;
- at least one hour of continuous operation.

The restricted WSL quarantine and stock M4 recovery boot are deliberately retained
until those checks pass.
