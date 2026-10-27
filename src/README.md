# A7 ImGui application source

`src/` is the complete Linux/A7 frontend. It contains no PC simulator, M4
firmware client or RPMsg transport.

## Runtime model

`main.cpp` loads `AppConfiguration`, constructs `Application` and exits with
its result. `Application` owns the Wayland/EGL window, ImGui lifecycle and the
background API, hardware and network workers. `ApplicationController` owns the
fixed frame order:

1. drain touch, RFID, API and network queues;
2. advance the attendance workflow;
3. route input to the current page;
4. apply navigation and timed transitions;
5. update backlight and feedback; and
6. render the active page.

Rendering never waits for SPI, HTTP, Wi-Fi/DHCP or PWM timing.

## Directory guide

| Directory | Responsibility |
|---|---|
| `app/` | Application lifetime, configuration, navigation, workflow and input routing |
| `pages/` | One class per visible page; isolated administrator panels and keyboard |
| `services/api/` | libcurl client, bounded worker, API result types and JSON parsing |
| `services/hardware/` | MFRC522 service, card-presence tracker and non-blocking buzzer sequences |
| `services/input/` | Linux touch parsing and coordinate rotation |
| `services/network/` | Asynchronous wpa_supplicant control socket and Wi-Fi parsing |
| `services/display/` | Backlight and performance metrics |
| `services/config/` | Atomic root-only administrator PIN persistence |
| `platform/linux/` | Wayland/EGL, SPI, GPIO, PWM, device discovery and diagnostics |
| `domain/` | Platform-independent UID, date and card-presence rules |
| `ui/` | Theme, drawing, signature SVG, assets, icons and transitions |

## Page and workflow state

Pages own only the state they render. `WorkflowSession` contains the small set
of data that must cross a screen transition: pending UID, user display data,
requested action, API request ID, health state and the card-removal guard.
Pages emit typed actions; they do not call unrelated services directly.

```text
Waiting → Processing → [clock-out Success]
                     ↘ [clock-in Attendance → Signature → Success]
```

The administrator area requires the configured four-digit PIN and has RFID,
PIN rotation, Wi-Fi and API address tabs. The tab row shares the back button's
row; `AdminPage` draws the panels 35 px above their own coordinates and maps
touch points back, while the full-screen keyboard uses screen coordinates. It clears temporary credentials after inactivity.

## Hardware behaviour

`HardwareService` discovers the A7 SPI device, resets the reader through PD15,
polls for cards and retries a disconnected reader without freezing the GUI.
`CardPresenceTracker` emits one UID presentation and requires removal before
the same card can trigger a new transaction.

`BuzzerSequencer` plays sparse semantic cues: `Tap`, `CardAccepted`, `Success`,
`Warning` and `Error`. PWM is stopped at the end of a sequence, on cancellation
and again from the systemd shutdown hook.

## Safe change rules

- Keep page drawing and render code free of blocking I/O.
- Introduce a small interface and fake before adding a new hardware/API dependency.
- Keep navigation in `ApplicationController` and `WorkflowCoordinator`.
- Preserve one-card-per-presentation behaviour.
- Never add secrets, M4 ELF files, generated builds or legacy RPMsg code.
- Run `bash scripts/test-host.sh` after source changes and
  `bash scripts/build-arm.sh` before deployment.

See [docs/DEVICE_FROM_SCRATCH.md](../docs/DEVICE_FROM_SCRATCH.md) for the
complete operating procedure and
[docs/BACKEND_INTEGRATION.md](../docs/BACKEND_INTEGRATION.md) for API details.
