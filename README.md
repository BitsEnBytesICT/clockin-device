# Clock-in Device A7 Client

This directory contains the complete STM32MP157F-DK2 clock-in application. The
GUI, RFID reader, buzzer, touchscreen, backlight and API client all run on the
Cortex-A7 under OpenSTLinux. No Cortex-M4 application, RPMsg channel, desktop
GUI simulator or compiled credential is required.

The application remains an 800×480 fullscreen ImGui kiosk. It preserves the
existing clock-in, attendance, signature, clock-out, administrator, error,
touchscreen, buzzer and backlight behavior while separating each concern into a
small production component.

## Repository layout

```text
src/app/                 Application lifetime, controller, navigation, workflow
src/pages/               One class and private state per screen
src/services/api/        Bounded asynchronous API client and JSON parser
src/services/hardware/   RFID and non-blocking buzzer services
src/services/input/      Linux multitouch parsing and coordinate rotation
src/services/display/    Backlight and optional performance metrics
src/platform/linux/      SPI, GPIO, PWM, Wayland/EGL and board discovery
src/domain/              Platform-independent UID/card rules
src/ui/                  Drawing helpers, theme, signature and embedded assets
tests/                   Headless unit tests and test doubles
board/                   Device tree, systemd unit and recovery resources
scripts/                 Host tests, ARM build, deployment and rollback
third_party/             Pinned minimal Dear ImGui and GLFW sources/licenses
docs/                    Architecture, board setup, operations and test evidence
```

Generated builds never belong in this tree. WSL writes them to
`~/.cache/clockin-device/host` and `~/.cache/clockin-device/arm`.

## Build and test

From Ubuntu 24.04 in WSL, using the Windows directory through
`/home/derk/clockin-device`:

```bash
cd /home/derk/clockin-device
bash scripts/test-host.sh
bash scripts/build-arm.sh
```

The first command runs headless tests with AddressSanitizer and
UndefinedBehaviorSanitizer. It never creates a window. The second command uses
the installed OpenSTLinux SDK and creates:

```text
~/.cache/clockin-device/arm/clockin-device
~/.cache/clockin-device/arm/clockin-hardware-diagnostic
```

## Deploy

The first deployment needs the current values supplied as process environment
variables. They are written directly to protected files and are not added to
the source tree or printed:

```bash
export CLOCKIN_API_BASE_URL='http://YOUR_API_HOST:3000'
export CLOCKIN_API_KEY='YOUR_KEY'
export CLOCKIN_ADMIN_PIN='FOUR_DIGITS'
bash scripts/deploy.sh root@192.168.11.51
unset CLOCKIN_API_KEY CLOCKIN_ADMIN_PIN
```

Later deployments preserve those board files when the variables are omitted.
The single device-owned service is `clockin-device.service`; ordinary operating
system services such as Weston remain unchanged.

Inspect the application with:

```bash
ssh root@192.168.11.51 systemctl status clockin-device.service
ssh root@192.168.11.51 journalctl -u clockin-device.service -b --no-pager
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/BOARD_SETUP.md](docs/BOARD_SETUP.md),
[docs/OPERATIONS.md](docs/OPERATIONS.md) and
[docs/TESTING.md](docs/TESTING.md) before changing board or boot resources.

## Dependencies and licenses

Dear ImGui and GLFW are built from the pinned source under `third_party`; their
licenses are stored beside the code. The embedded Outfit font subset is covered
by the SIL Open Font License in `src/ui/assets/OFL.txt`.
#