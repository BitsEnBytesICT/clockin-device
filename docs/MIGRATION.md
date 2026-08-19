# Refactor and migration notes

The previous application combined page rendering, input, workflow, API access,
backlight, simulator behavior and application lifetime in one `main.cpp` plus
large header-only helpers. It also kept duplicate WSL and Windows source trees.

The refactor made the Windows `IMGUI` directory authoritative and introduced:

- `Application` for process/display lifetime;
- `ApplicationController` for deterministic updates and navigation;
- `WorkflowCoordinator` plus `WorkflowSession` for cross-screen attendance state;
- one private class per page;
- injected API, hardware, SPI and buzzer interfaces;
- Linux-only SPI/GPIO/PWM/touch/display adapters;
- bounded API/hardware workers and a non-blocking buzzer sequencer;
- protected file-based board configuration;
- headless host tests and external build caches.

Removed functionality includes the WSLg simulator, keyboard card injection,
mock API delays/failures, screenshots, desktop/X11 build mode, RPMsg,
`/dev/ttyRPMSG0`, M4/ELF application artifacts, LED commands, card block
authentication/read/write, duplicated headers, generated source-tree builds,
Windows command scripts and copied website/backend material.

Required board resources remain under `board`: the A7 device-tree overlay,
dual-entry recovery boot configuration, systemd unit and PWM shutdown hook.
The old M4 runtime is retained only on the board as a temporary recovery path
until physical acceptance and soak testing are complete.
