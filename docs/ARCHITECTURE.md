# Architecture

## Runtime ownership

`main.cpp` only loads configuration, constructs `Application` and returns its
exit code. `Application` owns the Linux display/window, ImGui lifetime, API
worker, hardware worker and `ApplicationController`.

`ApplicationController` is the sole owner of update order and navigation. It
does not expose a mutable application context to pages. Each page owns only the
state it renders:

- `WaitingPage`: API status.
- `ProcessingPage`: progress message.
- `AttendancePage`: employee, dates, warning and scroll position.
- `SignaturePage`: employee, validation warning and signature pad.
- `SuccessPage` and `ErrorPage`: result text.
- `AdminPinPage`: PIN entry state.
- `AdminPage`: last scanned UID.

Information that must survive a screen transition lives in `WorkflowSession`:
the pending UID, employee data, action, active request and card-removal guard.
Pages return `UiControl` actions through `InputRouter`; they never call the API,
hardware service or another page directly.

## Deterministic frame cycle

Every rendered frame follows one order:

1. Poll Linux touch events and drain queued RFID/API results.
2. Advance the attendance workflow.
3. Route touch events to the active page.
4. Apply navigation and timed success/error transitions.
5. Update activity-driven backlight state and hardware feedback.
6. Render the active page and optional performance overlay.

The render thread never blocks on SPI, PWM timing or HTTP. `ApiWorker` and
`HardwareService` each own one background thread and bounded queues. Service
interfaces (`IApiService`, `IHardwareService`, `IBuzzerOutput`, `SpiTransport`)
allow tests to use small deterministic fakes.

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

`BuzzerSequencer` is a platform-independent state machine. A single beep is one
50 ms pulse. A triple buzz is three 50 ms pulses separated by 50 ms gaps. Linux
PWM output uses a 500,000 ns period (2 kHz) and 100,000 ns duty cycle (20%). The
sequence is advanced asynchronously and always disables PWM on completion,
cancellation, service stop and the systemd `ExecStopPost` safety hook.

## API safety

The queue is bounded and one worker reuses a libcurl handle. Scan and signed
clock-in requests are never automatically repeated. A request that may have
reached the server but lost its response is reported as uncertain and requires
a fresh card presentation; this avoids duplicate or reversed attendance state.

Configuration is loaded from environment/file references. The binary contains
no default API key or administrator PIN.
