# STM32MP157F-DK2 board setup

## Wiring retained by the A7 application

| Hardware | Function | STM32 pin | Connector |
|---|---|---:|---|
| MFRC522 | SPI5 MOSI | PF9 | Raspberry Pi header pin 19 |
| MFRC522 | SPI5 MISO | PF8 | Raspberry Pi header pin 21 |
| MFRC522 | SPI5 SCK | PH6 | Arduino D9 |
| MFRC522 | active-low software CS | PD14 | Arduino D3 |
| MFRC522 | reset | PD15 | Arduino D5 |
| MFRC522 | supply | 3.3 V | RPi pin 1 or 17 |
| MFRC522 | ground | GND | any ground |
| Buzzer | TIM16_CH1 PWM | PF6 | Raspberry Pi header pin 24 |
| Buzzer | ground | GND | any ground |

The PH6 clock is a deliberate custom mapping. Do not replace it with the DK2
header's more common PF7 SPI clock unless the physical wire is also moved.

## Moving ownership from M4 to Linux

`board/device-tree/stm32mp157f-dk2-clockin-overlay.dts` assigns PF8, PF9 and PH6
to SPI5 alternate function 5, PD14 as its active-low GPIO chip select, and PF6
to TIM16 channel 1. PD15 stays a GPIO requested by the application through
libgpiod. The overlay enables a spidev-compatible RFID child at 3 MHz and the
TIM16 PWM provider.

The `clockin-a7-test` boot entry selects the merged DTB and adds
`systemd.mask=m4-autostart.service`. This prevents the M4 firmware from taking
the same peripherals. The `stock-m4` boot entry and original DTB remain the
serial-console recovery path during the soak period.

To rebuild a merged DTB, first copy the exact board stock DTB to
`board/device-tree/build/stm32mp157f-dk2-stock.dtb`, then run:

```bash
bash board/device-tree/build-device-tree.sh
```

Boot resources change earlier than the application and can interrupt SSH if
incorrect. Use `scripts/deploy-a7-boot.sh` only while serial/ST-LINK recovery is
available. `scripts/rollback-stock-boot.sh` restores the saved extlinux files.

## Protected device configuration

The deployment creates:

```text
/etc/clockin-device/environment
/etc/clockin-device/rfid-api-key
/etc/clockin-device/admin-pin
```

All three files are owned by root and mode `0600`; the containing directory is
mode `0700`. `environment` references the two secret paths. The key and PIN are
never arguments to the application, part of the service unit or journalled.

The admin PIN can be changed on the device. The application writes a mode-`0600`
temporary file, flushes it, atomically renames it to `admin-pin`, and flushes the
containing directory before accepting the new PIN.

## Wi-Fi control prerequisite

OpenSTLinux continues to own Wi-Fi through its existing
`wpa_supplicant@wlan0.service` and `udhcpc-wlan0.service`; no NetworkManager or
ConnMan package is installed. Run this once before deploying the Wi-Fi-enabled
application:

```bash
bash scripts/migrate-network-control.sh root@10.10.1.20
```

The migration sanitizes obsolete commented credential lines, enables a
root-only control socket at `/run/wpa_supplicant/wlan0`, and enables
`SAVE_CONFIG`. It preserves all live network blocks and the existing regulatory
country. It restarts Wi-Fi/DHCP in a transient systemd task, waits up to 45
seconds for association and IPv4, and restores the exact old configuration if
validation fails. Its temporary backup is deleted after success.

## Installed runtime files

```text
/usr/local/bin/clockin-device
/usr/local/bin/clockin-hardware-diagnostic
/usr/local/lib/clockin-device/disable-buzzer.sh
/etc/systemd/system/clockin-device.service
/etc/default/demo-launcher
```

`imgui-app.service` and `m4-autostart.service` are disabled once the new service
is installed. Weston and normal OpenSTLinux services remain.

## Boot-persistent kiosk service

`clockin-device.service` is enabled under `multi-user.target`, which is the
board's default target. It orders itself after and requests
`weston-graphical-session.service`, `wpa_supplicant@wlan0.service` and
`udhcpc-wlan0.service`, then waits for the actual Wayland socket at
`/run/user/1000/wayland-0`. This replaces the old fixed 20-second startup delay
and the obsolete dependency on `m4-autostart.service`.

The project installs a small drop-in for
`weston-graphical-session.service`. It selects `tty7` in Weston's own
pre-start phase, before the Weston logind session and DRM compositor are
created. This prevents the `tty1` console from obscuring the kiosk without
deactivating a running DRM compositor.

The deployment also writes `/etc/default/demo-launcher` with a harmless
`/bin/true` command. This uses the OpenSTLinux demo launcher's own supported
configuration hook to suppress the vendor home/demo application, ensuring that
it cannot cover the clock-in UI after boot.

The application runs as root because SPI, GPIO, PWM, input and backlight access
are board-level responsibilities. `Restart=always`, a five-second delay and an
unlimited start interval keep the kiosk alive after an unexpected clean exit,
crash, unavailable initial Wayland session or repeated failure. A normal
`systemctl stop clockin-device.service` remains authoritative and does not
trigger a restart. Output is retained in the system journal, and
`disable-buzzer.sh` runs after every exit.

Verify the installed boot configuration with:

```bash
systemctl is-enabled clockin-device.service
systemctl show clockin-device.service -p ActiveState -p SubState -p Restart -p NRestarts
systemctl status weston-graphical-session.service clockin-device.service
```

The Wi-Fi services are `Wants`, not `Requires`: a missing access point, DHCP
lease or management server never prevents the kiosk UI from starting.
