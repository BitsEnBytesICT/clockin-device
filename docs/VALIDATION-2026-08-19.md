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
| `clockin-device` | `b116e67517428356b074507eaa0943d7eeb5448152af5ec97558b174da3e6193` |
| `clockin-hardware-diagnostic` | `047b03124c6a65d582b3397f5c27efabcc9c9d0a7028d929aaab8eab2c5897fa` |

- Board configuration directory is root-owned mode `0700`; environment, API key and PIN files are root-owned mode `0600`.
- Journal scan found no credential values.
- Display initialized as RGB565 (`R5 G6 B5 A0`) at 50 Hz.
- Touch initialized at `/dev/input/event1`.
- MFRC522 initialized on `/dev/spidev0.0`, version register `0x92`.
- Diagnostic emitted the single/triple buzzer patterns and left PWM disabled.
- `imgui-app.service` is disabled and inactive.
- `m4-autostart.service` is masked and inactive.
- `clockin-device.service` is enabled, active and has zero restarts.
- Clean service stop completed in 0.87 seconds and logged both shutdown stages.
- A stop while the API connection was in progress also completed cleanly; the PWM channel was unexported.

## Reboot evidence

Three consecutive boots reached the A7 application with hardware initialized and
the service active with zero restarts:

1. `6006fe4d-53e4-4258-81a7-b41efa413c9a`
2. `c63b2dbb-9b85-4dd6-8918-27bc12690270`
3. `9450d1e2-c4d7-49da-a4da-ccb437b86735`

## Pending manual/connected checks

The configured API at `http://192.168.11.194:3000` timed out during validation.
Consequently, full clock-in/out, attendance and signed workflow checks remain
pending until that backend is running. A person at the device must also confirm:

- ten repeat UID presentations and held-card behavior;
- all touchscreen corners, scrolling and signature drawing;
- visible admin, success and error screens;
- backlight dim/off/wake timings;
- behavior while the RFID reader is physically disconnected;
- audible quality of the newly deployed buzzer patterns;
- at least one hour of continuous operation.

The restricted WSL quarantine and stock M4 recovery boot are deliberately retained
until those checks pass.
