# Build, deployment and recovery scripts

Run scripts from WSL/Linux, not PowerShell. They find the repository root from
their own path and keep generated build output outside the checkout.

| Script | Purpose | Use it when |
|---|---|---|
| `test-host.sh` | Headless unit tests with ASan/UBSan | Before every release or refactor |
| `build-arm.sh` | SDK cross-build of kiosk and hardware diagnostic | Before deploying |
| `deploy-a7-boot.sh` | Back up and install A7 DTB/boot resources | First A7 conversion only, with serial recovery |
| `rollback-stock-boot.sh` | Restore saved stock boot menu | A7 DT/boot recovery |
| `migrate-network-control.sh` | Enable verified wpa_supplicant control access | Once, before Wi-Fi UI use |
| `deploy.sh` | Install binaries, services, kiosk config and protected board configuration | Normal releases |

## Normal release sequence

```bash
bash scripts/test-host.sh
bash scripts/build-arm.sh
bash scripts/migrate-network-control.sh root@10.10.1.20  # one time only
bash scripts/deploy.sh root@10.10.1.20
```

The first deployment needs `CLOCKIN_API_BASE_URL`, `CLOCKIN_API_KEY` and
`CLOCKIN_ADMIN_PIN` in the current shell. Later releases preserve the existing
root-only values when they are omitted.

## Optional overrides

```bash
export CLOCKIN_HOST_BUILD_DIR=/path/to/host-build
export CLOCKIN_ARM_BUILD_DIR=/path/to/arm-build
export STM32_SDK_ENV=/path/to/environment-setup-...
export CLOCKIN_BOARD=root@10.10.1.20
```

## Safety rules

- Never run `deploy-a7-boot.sh` without serial/ST-LINK recovery.
- `rollback-stock-boot.sh` restores the M4 boot path; it is not an application
  rollback. See `docs/OPERATIONS.md` for application rollback.
- Do not commit environment values or copy secrets into scripts.
- Deployment intentionally fails when the A7 boot mask, SPI device or PWM
  provider is missing; fix boot resources first.

Use [docs/DEVICE_FROM_SCRATCH.md](../docs/DEVICE_FROM_SCRATCH.md) for full
setup and [docs/OPERATIONS.md](../docs/OPERATIONS.md) for live recovery.
