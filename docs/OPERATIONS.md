# Build, deploy and recovery operations

## Source and build locations

The Windows `IMGUI` directory is the only editable source. In WSL it is exposed
through `/home/derk/clockin-device`. Do not copy it into a second Linux source
tree. Generated builds stay outside the source:

```text
/home/derk/.cache/clockin-device/host
/home/derk/.cache/clockin-device/arm
```

## Normal release sequence

```bash
cd /home/derk/clockin-device
bash scripts/test-host.sh
bash scripts/build-arm.sh
```

The ARM build script finds the latest SDK below `/opt/st/stm32mp1`. Set
`STM32_SDK_ENV` to select a specific environment setup file. It verifies the
output architecture and scans the production binary for legacy simulator,
RPMsg and LED command strings.

For an already-configured board:

```bash
bash scripts/deploy.sh root@192.168.11.51
```

For the first deployment, export `CLOCKIN_API_BASE_URL`, `CLOCKIN_API_KEY` and
`CLOCKIN_ADMIN_PIN` first. The script preserves an existing board configuration
when those variables are absent and backs up the previous executable and unit
under `/usr/local/lib/clockin-device/backup`.

## Diagnosis

```bash
ssh root@192.168.11.51 systemctl status clockin-device.service
ssh root@192.168.11.51 journalctl -u clockin-device.service -b --no-pager
ssh root@192.168.11.51 /usr/local/bin/clockin-hardware-diagnostic --buzzer-only
ssh root@192.168.11.51 /usr/local/bin/clockin-hardware-diagnostic --rfid-only --rfid-seconds=60
```

Useful read-only checks:

```bash
ssh root@192.168.11.51 'cat /proc/cmdline; ls -l /dev/spidev* /sys/class/pwm/pwmchip*'
ssh root@192.168.11.51 'systemctl show clockin-device.service -p ActiveState -p SubState -p MainPID -p NRestarts -p Result'
```

Do not print `/etc/clockin-device/rfid-api-key` or `admin-pin` into a terminal or
journal. To rotate either value, write it through a protected input method,
enforce root ownership/mode `0600`, then restart the service.

## Application rollback

The deployment backup can restore the previous A7 application without changing
the boot resources:

```bash
ssh root@192.168.11.51 'systemctl stop clockin-device.service && \
  cp -p /usr/local/lib/clockin-device/backup/clockin-device.previous /usr/local/bin/clockin-device && \
  cp -p /usr/local/lib/clockin-device/backup/clockin-device.service.previous /etc/systemd/system/clockin-device.service && \
  systemctl daemon-reload && systemctl start clockin-device.service'
```

If A7 boot resources themselves are the problem, run
`scripts/rollback-stock-boot.sh`, reboot, and select the stock M4 entry over the
serial console.
