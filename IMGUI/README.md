# RFID attendance ImGui client

This is the touchscreen attendance client for the **STM32MP157F-DK2**. It reads
RFID cards from the board's M4 firmware over RPMsg, shows the clock-in/signature
workflow, calls the Bits & Bytes management API, and controls the device LEDs,
buzzer, touchscreen and backlight.

The same source also builds as a WSLg desktop simulator on Windows. In simulator
mode, only the board hardware is simulated; API calls can still go to a local,
acceptance or production backend.

The DK2 has a dual 800 MHz Cortex-A7, 512 MB DDR3L and a Vivante OpenGL ES 2.0
GPU. Its 800x480 panel is exposed by the ST Linux driver at 50 Hz (preferred)
and 60 Hz. The application keeps VSync enabled and follows the mode selected by
Weston/DRM; it does not force the panel to an unverified refresh rate.

## Included

- Application source with embedded Outfit Regular/SemiBold subsets
- Bits & Bytes colours and six monochrome management-platform icons packed into
  the same 512-wide ImGui font texture
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

The mock can emulate a slow or failing backend without blocking rendering:

```bash
STM32_SIM_MOCK_API=1 STM32_SIM_API_DELAY_MS=10000 ./run-desktop-sim.sh
STM32_SIM_MOCK_API=1 STM32_SIM_API_FAILURE=uncertain ./run-desktop-sim.sh
```

Supported failure values are `offline`, `uncertain`, `malformed`, `auth`, and
`http500`. They are simulator-only.

Automated performance smoke runs can close the simulator cleanly and populate a
worst-case active signature without a mouse:

```bash
STM32_SIM_MOCK_API=1 STM32_SIM_API_DELAY_MS=10000 \
  BITS_BYTES_PERF_OVERLAY=1 ./build-desktop/imgui_app \
  --sim-rfid=SIM_CLOCK_OUT --sim-exit-after=5

STM32_SIM_MOCK_API=1 BITS_BYTES_PERF_OVERLAY=1 \
  ./build-desktop/imgui_app --sim-signature-points=1000 --sim-exit-after=5

# Capture an unobstructed native 800x480 UI frame as a portable PPM image.
STM32_SIM_MOCK_API=1 ./build-desktop/imgui_app \
  --sim-screenshot=/tmp/bits-bytes-ui.ppm --sim-exit-after=2
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
`/dev/input/event1` for touch on the target board. Missing hardware devices are
retried with bounded backoff, so the UI can start while RPMsg or touch is still
being initialized.

## Runtime diagnostics and tuning

Optional settings:

```bash
# Show FPS, frame percentiles, touch latency, draw size and reconnect counters.
export BITS_BYTES_PERF_OVERLAY=1

# Override the touch event device when Linux enumerates it differently.
export BITS_BYTES_TOUCH_DEVICE=/dev/input/event1

# auto tries RGB565 on STM32 and falls back to RGBA8888.
export BITS_BYTES_FRAMEBUFFER_FORMAT=auto  # auto | rgb565 | rgba8888
```

At startup the program logs the active refresh rate and the framebuffer channel
bits actually selected by EGL. `auto` defaults to RGBA8888 in the desktop
simulator and RGB565-with-fallback on STM32. Touch orientation and the existing
180-degree STM32 display rotation remain unchanged.

Network calls run on one background worker and reuse their TLS connection. The
RFID scan and signed clock-in routes are intentionally never retried: if a
request may have reached the server, its reply was lost, or a state-changing
route returns a possibly-partial 5xx response, the UI says
`Resultaat onbekend` and requires removal/fresh presentation of the card. This
prevents an automatic retry from reversing or duplicating attendance state.

M4 commands use the persistent RPMsg descriptor. Only one command is in flight;
the scheduler waits for the firmware's existing `RX:` echo and applies safe
post-buzzer spacing before sending another command. This avoids command loss
while the M4 is temporarily blocked scanning a held card.

The embedded UI follows the management platform's Outfit typography and colour
tokens. Action controls use dark-blue primary, blue-outline secondary and quiet
white variants; red and green are reserved for error and success status. Button
icons are generated offline into 20x20 alpha masks and packed into the existing
font atlas. The board does not parse SVGs, decode images, load fonts from disk or
switch to a second UI texture. Touch-down scales only the drawn rectangle; the
original 140x60 hit areas and release validation remain unchanged.

## Tests

The desktop build includes tests for touch report parsing and rotation,
signature smoothing/error and payload limits, JSON escaping, fragmented RPMsg,
held-card timing, hardware command spacing, async API delays, cancellation,
stale replies and ambiguous results:

```bash
cmake -S . -B build-desktop -DDESKTOP_SIM=ON -DIMGUI_BUILD_TESTS=ON
cmake --build build-desktop --parallel
ctest --test-dir build-desktop --output-on-failure
```

The test executable also checks the bounded button press/release/cancel
animation state. UI asset generation is not part of either normal build. To
regenerate assets after changing the management-platform design sources, install
`fonttools`, `Pillow` and `CairoSVG`, then run:

```bash
python3 tools/generate_branded_assets.py \
  --frontend /path/to/Bits-Bytes-management-platform/frontend
```

For memory/undefined-behavior checks, configure a separate build after the
desktop launcher has prepared its local curl headers. A bounded workflow soak
runner is included:

```bash
cmake -S . -B build-sanitize -DDESKTOP_SIM=ON -DIMGUI_BUILD_TESTS=ON \
  -DENABLE_SANITIZERS=ON \
  -DLOCAL_CURL_ROOT="$PWD/build-desktop/curl-dev/root"
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1 ./build-sanitize/imgui_tests --soak-seconds=1800
```

Before deploying a release to the board, run `./build-stm32.sh` and verify that
`file build-stm32/imgui_app` reports a 32-bit ARM EABI5 executable.

## DK2 verification checklist

- Confirm the startup log reports the intended 50 or 60 Hz mode with VSync.
- Draw while a test API response is delayed; the trace must continue updating.
- Check all four touch corners and the signature/button boundary.
- Hold one RFID card in the field; it must create exactly one workflow.
- Remove and present it again; the next workflow must be accepted.
- Exercise clock-in, attendance, signature, clock-out, cancel and uncertain-result paths.
- Verify red/green LEDs, buzzer, backlight dimming and RPMsg reconnect behavior.
- With the overlay enabled, target p95 frame time below 25 ms at 50 Hz or
  20.8 ms at 60 Hz, p99 below two frames, and touch latency below two frames.
- Complete the board soak before release; simulator/cross-build validation does
  not substitute for measuring the real DK2 display and touchscreen.

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

Outfit is Copyright 2021 The Outfit Project Authors and distributed under the
SIL Open Font License 1.1 in `assets/fonts/OFL.txt`. The embedded files are
static, Latin-focused subsets generated from the management platform's checked-in
variable font.

Board/display references: [STM32MP157F-DK2 product page](https://www.st.com/en/evaluation-tools/stm32mp157f-dk2.html),
[ST GPU application programming manual](https://www.st.com/resource/en/programming_manual/pm0263-stm32mp157-gpu-application-programming-manual-stmicroelectronics.pdf),
and the [ST 50/60 Hz panel driver](https://github.com/STMicroelectronics/linux/blob/v6.6-stm32mp/drivers/gpu/drm/panel/panel-orisetech-otm8009a.c).
