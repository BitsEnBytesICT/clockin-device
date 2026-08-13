# RFID attendance ImGui client

This is the touchscreen attendance client for the **STM32MP157F-DK2**. It reads
RFID cards from the board's M4 firmware over RPMsg, shows the clock-in/signature
workflow, calls the Bits & Bytes management API, and controls the device LEDs,
buzzer, touchscreen and backlight.

The same source also builds as a WSLg desktop simulator on Windows. In simulator
mode, only the board hardware is simulated; API calls can still go to a local,
acceptance or production backend.

## Included

- Application source and embedded Plus Jakarta Sans font
- Pinned ImGui and GLFW source needed to build without generated binaries
- WSL desktop build/launcher
- STM32MP1 OpenSTLinux cross-build launcher

Build folders, API keys, private certificates and machine-specific files are
intentionally excluded.

## Desktop simulator on Windows/WSL

Install Ubuntu 24.04 under WSL2 with WSLg, then install build dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config dpkg-dev \
  libegl1-mesa-dev libgles2-mesa-dev libwayland-dev wayland-protocols \
  libxkbcommon-dev xorg-dev
```

From this repository:

```bash
cd IMGUI
BITS_BYTES_API_BASE_URL=https://management-acc.bitsenbytes.net \
BITS_BYTES_API_KEY='YOUR_ACC_API_KEY' \
./run-desktop-sim.sh
```

The API key is sent directly in the `Authorization` header, without `Bearer`.
Never add a real API key to Git. Always set the URL explicitly for ACC testing;
the source's compile-time fallback remains the production URL.

For offline UI work, use the built-in mock backend:

```bash
STM32_SIM_MOCK_API=1 ./run-desktop-sim.sh
```

Simulator controls:

- Mouse: touchscreen
- F1/F2: scan RFID `11F3EF12`
- F3: simulate an unknown card
- F4: inject a card without resetting the screen
- F5: reset to the waiting screen
- F12: toggle the help overlay

To test against a backend running on Windows, use its WSL-reachable host address,
for example `BITS_BYTES_API_BASE_URL=http://192.168.224.1:3000`.

## Build for STM32MP157F-DK2

Install ST's STM32MP1 OpenSTLinux Weston SDK (Linux 6.6/scarthgap) under
`/opt/st/stm32mp1`, then run:

```bash
cd IMGUI
./build-stm32.sh
```

If the SDK is elsewhere:

```bash
STM32_SDK_ENV=/path/to/environment-setup-cortexa7... ./build-stm32.sh
```

The result is `build-stm32/imgui_app`. Verify that `file` reports a 32-bit ARM
EABI Linux executable, then deploy it:

```bash
scp build-stm32/imgui_app root@BOARD_IP:/usr/local/bin/imgui_app
ssh root@BOARD_IP chmod +x /usr/local/bin/imgui_app
```

Configure the board at runtime so the same binary can move between environments:

```bash
export BITS_BYTES_API_BASE_URL=https://management-acc.bitsenbytes.net
export BITS_BYTES_API_KEY_FILE=/etc/bitsenbytes/rfid-api-key
/usr/local/bin/imgui_app
```

Store only the raw key in `/etc/bitsenbytes/rfid-api-key` and protect it with
`chmod 600`. The client uses `/dev/ttyRPMSG0` for RFID/M4 communication and
`/dev/input/event1` for touch on the target board.

## Backend contract

The client uses:

- `POST /api/scan`
- `POST /api/clock_in_with_signature`
- `POST /api/attendance_last_30`

All attendance calls require an authorized API key. The matching backend is the
[Bits & Bytes management platform](https://github.com/BitsEnBytesICT/Bits-Bytes-management-platform).

The vendored dependencies are Dear ImGui at commit `1897248bda4873654bc79cddebb8a9119f16467a`
and GLFW at commit `8e15281d34a8b9ee9271ccce38177a3d812456f8`. Their licenses are included
beside their source.
