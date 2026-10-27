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
src/services/network/    Asynchronous wpa_supplicant control and Wi-Fi parsing
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

## Host prerequisites

On Windows, install WSL 2 with Ubuntu 24.04. Inside Ubuntu, install the normal
host build tools:

```bash
sudo apt update
sudo apt install -y git cmake build-essential file binutils
```

Production builds require the matching OpenSTLinux 5.0.8 STM32MP1 SDK. Install
it below `/opt/st/stm32mp1`; `scripts/build-arm.sh` finds its
`environment-setup-*` file automatically. If it is installed elsewhere, set
`STM32_SDK_ENV` to the full environment-setup path before building. The SDK is
not stored in Git because it is large and machine-specific.

## Build and test

Clone the repository, then run the scripts from any source location visible to
Ubuntu/WSL:

```bash
git clone https://github.com/BitsEnBytesICT/attendance-device.git
cd attendance-device
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
export CLOCKIN_API_BASE_URL='https://management-acc.bitsenbytes.net'
export CLOCKIN_API_KEY='YOUR_KEY'
export CLOCKIN_ADMIN_PIN='FOUR_DIGITS'
bash scripts/deploy.sh root@10.10.1.20
unset CLOCKIN_API_KEY CLOCKIN_ADMIN_PIN
```

Later deployments preserve those board files when the variables are omitted.
The single device-owned service is `clockin-device.service`; ordinary operating
system services such as Weston remain unchanged.

Before the first release containing on-screen Wi-Fi management, enable the
existing `wpa_supplicant` control socket once. The migration preserves the
active network, waits for association and IPv4, and automatically restores the
original configuration if either check fails:

```bash
bash scripts/migrate-network-control.sh root@10.10.1.20
```

The administrator screen then provides three tabs: RFID diagnostics, protected
four-digit PIN rotation, and Wi-Fi management. Wi-Fi changes happen on a
background worker so rendering and RFID remain responsive. See
[docs/NETWORKING.md](docs/NETWORKING.md) for security and recovery details.

## Boot-persistent kiosk

Deployment enables `clockin-device.service` in the board's default
`multi-user.target`. The service requests `weston-graphical-session.service`,
waits up to 60 seconds for `/run/user/1000/wayland-0`, and then launches the
fullscreen application. A Weston service drop-in selects virtual terminal
`tty7` before Weston's graphical session starts, so the compositor keeps its
DRM and input seat. The kiosk uses `Restart=always` with a five-second delay and
has no systemd restart-rate cutoff, so both crashes and clean unexpected exits
recover automatically. A deliberate `systemctl stop` still leaves it stopped.

Logs are stored in the system journal rather than an unbounded file in `/root`.
The buzzer cleanup runs whenever the application exits.

Inspect the application with:

```bash
ssh root@10.10.1.20 systemctl is-enabled clockin-device.service
ssh root@10.10.1.20 systemctl status clockin-device.service
ssh root@10.10.1.20 journalctl -u clockin-device.service -b --no-pager
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/BOARD_SETUP.md](docs/BOARD_SETUP.md),
[docs/NETWORKING.md](docs/NETWORKING.md),
[docs/OPERATIONS.md](docs/OPERATIONS.md) and
[docs/TESTING.md](docs/TESTING.md) before changing board or boot resources. The
latest admin/network evidence and remaining physical checks are recorded in
[docs/VALIDATION-2026-08-26-ADMIN.md](docs/VALIDATION-2026-08-26-ADMIN.md).

## Dependencies and licenses

Dear ImGui and GLFW are built from the pinned source under `third_party`; their
licenses are stored beside the code. The embedded Outfit font subset is covered
by the SIL Open Font License in `src/ui/assets/OFL.txt`.
