# Board, pins, boot resources and kiosk units

`board/` contains the non-secret files that make the DK2 an A7/Linux clock-in
device.

## Wiring

| Device | Signal | STM32 pin | DK2 connector |
|---|---|---:|---|
| MFRC522 | MOSI | PF9 / SPI5 AF5 | Raspberry Pi header pin 19 |
| MFRC522 | MISO | PF8 / SPI5 AF5 | Raspberry Pi header pin 21 |
| MFRC522 | SCK | PH6 / SPI5 AF5 | Arduino D9 |
| MFRC522 | active-low CS | PD14 / GPIO | Arduino D3 |
| MFRC522 | reset | PD15 / GPIO | Arduino D5 |
| MFRC522 | 3.3 V / GND | 3.3 V / GND | RPi pin 1 or 17 / any ground |
| Buzzer | PWM | PF6 / TIM16_CH1 AF1 | Raspberry Pi header pin 24 |
| Buzzer | GND | GND | any ground |

The MFRC522 uses 3.3 V. Its custom clock wire is PH6; do not substitute PF7
unless the physical wiring and device tree both change.

## Contents

| Path | Purpose |
|---|---|
| `device-tree/stm32mp157f-dk2-clockin-overlay.dts` | A7 SPI5, GPIO and TIM16 PWM ownership |
| `device-tree/build-device-tree.sh` | Merges overlay with the exact stock DTB |
| `device-tree/extlinux-clockin.conf` | A7 test and stock-M4 recovery boot entries |
| `systemd/clockin-device.service` | Single device-owned kiosk application service |
| `systemd/weston-graphical-session.service.d/` | Selects tty7 before Weston gets DRM/input ownership |
| `systemd/disable-buzzer.sh` | Safety shutdown of the TIM16 PWM output |
| `weston/demo-launcher.conf` | Suppresses the vendor ST home/demo launcher |
| `network/enable-wpa-control.sh` | Enables the protected wpa_supplicant control socket |

## A7 versus M4

The `clockin-a7-test` menu entry loads the merged DTB and masks
`m4-autostart.service`. The A7 app uses `/dev/spidev0.0`, GPIO and PWM directly;
it never opens `/dev/ttyRPMSG0`. The `stock-m4` boot entry remains the recovery
path while the A7 configuration is being proven.

## Display ownership

Weston owns the Wayland display. The drop-in selects `tty7` before Weston
creates its graphical session, then the kiosk service waits for the Wayland
socket. The demo launcher override prevents the OpenSTLinux home/demo app from
covering the ImGui surface. Do not call `chvt 7` after Weston is running.

For full installation and recovery instructions, read
[docs/DEVICE_FROM_SCRATCH.md](../docs/DEVICE_FROM_SCRATCH.md).
