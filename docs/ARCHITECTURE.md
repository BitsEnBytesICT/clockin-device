# Architecture

## Runtime ownership

`main.cpp` only loads configuration, constructs `Application` and returns its
exit code. `Application` owns the Linux display/window, ImGui lifetime, API
worker, hardware worker, network worker and `ApplicationController`.

`ApplicationController` is the sole owner of update order and navigation. It
does not expose a mutable application context to pages. Each page owns only the
state it renders:

- `WaitingPage`: API status.
- `ProcessingPage`: progress message.
- `AttendancePage`: employee, dates, warning and scroll position.
- `SignaturePage`: employee, validation warning and signature pad.
- `SuccessPage` and `ErrorPage`: result text.
- `AdminPinPage`: login PIN entry state.
- `AdminPage`: the admin shell and tab selection.
- `AdminRfidPanel`: last scanned UID and reader feedback.
- `AdminPinPanel`: the two-step PIN-change workflow.
- `AdminNetworkPanel`: Wi-Fi list, details, confirmations and status.
- `OnScreenKeyboard`: temporary Wi-Fi passphrase entry.

Information that must survive a screen transition lives in `WorkflowSession`:
the pending UID, employee data, action, active request and card-removal guard.
Pages return typed actions through `InputRouter`; they never call the API,
hardware or network service directly. `ApplicationController` executes those
actions and feeds immutable snapshots back to the active admin panel.

## Deterministic frame cycle

Every rendered frame follows one order:

1. Poll Linux touch events and drain queued RFID/API/network results.
2. Advance the attendance workflow.
3. Route touch events to the active page.
4. Apply navigation and timed success/error transitions.
5. Update activity-driven backlight state and hardware feedback.
6. Render the active page and optional performance overlay.

The render thread never blocks on SPI, PWM timing, HTTP, Wi-Fi association or
DHCP. `ApiWorker`, `HardwareService` and `NetworkService` each own one background
thread and bounded queues. Service interfaces (`IApiService`,
`IHardwareService`, `INetworkService`, `IBuzzerOutput`, `SpiTransport`) allow
tests to use small deterministic fakes.

## Administrator security

Opening the admin shell still requires the configured four-digit PIN. The shell
locks after 120 seconds without touch activity and wipes partially entered PINs
and Wi-Fi passwords. A PIN change is entered twice and is atomically written to
the configured root-only file before the in-memory value changes. An inline
`BITS_BYTES_ADMIN_PIN` override is deliberately read-only because a file update
would be shadowed at the next restart.

Wi-Fi passwords are validated, sent over the local Unix datagram control socket,
then overwritten in page, controller and worker command buffers. They are never
arguments to a process and never logged. Persistent credentials are managed by
`wpa_supplicant` in its root-owned mode-`0600` configuration.

## RFID behavior

`HardwareService` discovers the SPI node from the device-tree path, pulses PD15
through libgpiod, initializes the MFRC522, then polls for UIDs every 100 ms. A
reader failure closes the descriptors and retries with delay; it does not stop
the GUI.

`CardPresenceTracker` converts repeated MFRC522 detections into one presentation
event. A card becomes absent only after 1.5 seconds without a detection. The
workflow also records the UID that must be removed, preventing a held card from
creating a second attendance operation after a success or uncertain response.

Only UID cascade reads are implemented. Authentication, sector read/write and
card storage are intentionally absent.

## Buzzer behavior

`BuzzerSequencer` is a platform-independent state machine for five semantic
cues: a very short tap, card acknowledgement, rising success cue, neutral
warning and descending error cue. Notes remain close to the buzzer's proven
2 kHz range, use only 10-18% duty cycle and last 15-165 ms in total. Short silent
gaps separate notes cleanly without continuous PWM reconfiguration.

Sound is deliberately sparse. A new RFID presentation is acknowledged once;
completed attendance operations receive success feedback; user-facing workflow
errors receive error feedback. Only important transactional controls such as
network refresh/connect, attendance confirmation and signature submission use
the quiet tap. Destructive disconnect/forget actions use the warning. Tabs,
navigation, PIN digits and keyboard keys remain silent.

Low-priority duplicate taps are coalesced in a bounded queue, while success and
error feedback supersede waiting low-priority cues. The sequence advances on
the hardware thread and always disables PWM on completion, cancellation,
service stop and the systemd `ExecStopPost` safety hook.

## API safety

The queue is bounded and one worker reuses a libcurl handle. Scan and signed
clock-in requests are never automatically repeated. A request that may have
reached the server but lost its response is reported as uncertain and requires
a fresh card presentation; this avoids duplicate or reversed attendance state.

Configuration is loaded from environment/file references. The binary contains
no default API key or administrator PIN.
