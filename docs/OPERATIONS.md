# Build, deploy and recovery operations

## Source and build locations

The cloned `attendance-device` repository is the editable source. It may live
inside WSL or in a Windows directory exposed to WSL; every script discovers the
repository root from its own location. Generated builds stay outside the source:

```text
~/.cache/clockin-device/host
~/.cache/clockin-device/arm
```

## Normal release sequence

```bash
cd /path/to/attendance-device
bash scripts/test-host.sh
bash scripts/build-arm.sh
```

The ARM build script finds the latest SDK below `/opt/st/stm32mp1`. Set
`STM32_SDK_ENV` to select a specific environment setup file. It verifies the
output architecture and scans the production binary for legacy simulator,
RPMsg and LED command strings.

For an already-configured board:

```bash
bash scripts/migrate-network-control.sh root@10.10.1.20  # once
bash scripts/deploy.sh root@10.10.1.20
```

For the first deployment, export `CLOCKIN_API_BASE_URL`, `CLOCKIN_API_KEY` and
`CLOCKIN_ADMIN_PIN` first. The script preserves an existing board configuration
when those variables are absent and backs up the previous executable and unit
under `/usr/local/lib/clockin-device/backup`.

## Diagnosis

```bash
ssh root@10.10.1.20 systemctl status clockin-device.service
ssh root@10.10.1.20 journalctl -u clockin-device.service -b --no-pager
ssh root@10.10.1.20 /usr/local/bin/clockin-hardware-diagnostic --buzzer-only
ssh root@10.10.1.20 /usr/local/bin/clockin-hardware-diagnostic --rfid-only --rfid-seconds=60
```

Useful read-only checks:

```bash
ssh root@10.10.1.20 'cat /proc/cmdline; ls -l /dev/spidev* /sys/class/pwm/pwmchip*'
ssh root@10.10.1.20 'systemctl show clockin-device.service -p ActiveState -p SubState -p MainPID -p NRestarts -p Result'
```

## Kiosk startup and recovery

The application is enabled at boot and starts after Weston exposes its Wayland
socket. It restarts five seconds after any unexpected process exit. The journal
is the authoritative log; `/root/imgui.log` is no longer used.

```bash
ssh root@10.10.1.20 'systemctl is-enabled clockin-device.service; systemctl show clockin-device.service -p ActiveState -p SubState -p Restart -p MainPID -p NRestarts'
ssh root@10.10.1.20 'journalctl -u clockin-device.service -b --no-pager -n 100'
ssh root@10.10.1.20 'cat /sys/class/tty/tty0/active'
```

The final command should report `tty7`. If a stale console is ever visible,
restart Weston and then the kiosk so Weston's pre-start terminal selection is
applied before its compositor starts:

```bash
ssh root@10.10.1.20 'systemctl restart weston-graphical-session.service && systemctl restart clockin-device.service'
```

Do not run `chvt 7` after Weston is already running: that can deactivate its
DRM/input seat and leave the panel black.

To prove automatic recovery without rebooting, record `MainPID`, kill only the
managed process, wait at least five seconds, and confirm that `MainPID` changed
and `NRestarts` increased:

```bash
ssh root@10.10.1.20 'systemctl show clockin-device.service -p MainPID -p NRestarts; systemctl kill --kill-who=main --signal=SIGKILL clockin-device.service; sleep 8; systemctl show clockin-device.service -p ActiveState -p MainPID -p NRestarts'
```

`systemctl stop clockin-device.service` deliberately suppresses automatic
restart. Run `systemctl start clockin-device.service` or reboot to resume kiosk
operation.

Do not print `/etc/clockin-device/rfid-api-key` or `admin-pin` into a terminal or
journal. To rotate either value, write it through a protected input method,
enforce root ownership/mode `0600`, then restart the service.

## Application rollback

The deployment backup can restore the previous A7 application without changing
the boot resources:

```bash
ssh root@10.10.1.20 'systemctl stop clockin-device.service && \
  cp -p /usr/local/lib/clockin-device/backup/clockin-device.previous /usr/local/bin/clockin-device && \
  cp -p /usr/local/lib/clockin-device/backup/clockin-device.service.previous /etc/systemd/system/clockin-device.service && \
  systemctl daemon-reload && systemctl start clockin-device.service'
```

If A7 boot resources themselves are the problem, run
`scripts/rollback-stock-boot.sh`, reboot, and select the stock M4 entry over the
serial console.
