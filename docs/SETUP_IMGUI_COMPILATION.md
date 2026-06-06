# RFID Attendance System v2.1.2 — STM32MP157DK-2 Cross-Compilation Guide

## What This Is

A touchscreen GUI application for the **STM32MP157DK-2** Discovery Kit. Built with
Dear ImGui, it reads RFID cards, captures signatures, and communicates with a backend
API over HTTP.

---

## Quick Start (TL;DR)

If you already have WSL + the STM32MP1 SDK installed:

```powershell
# 1. Copy project into WSL
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    rm -rf ~/project && mkdir -p ~/project &&
    cp '\\wsl.localhost\Ubuntu-24.04\mnt\c\<path-to-your-project>\*' ~/project/ -r
"

# 2. Build
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    source /opt/st/stm32mp1/*/environment-setup-* &&
    cd ~/project &&
    cmake -B build -DCMAKE_BUILD_TYPE=Release &&
    cmake --build build -j \$(nproc)
"
```

The binary is at `~/project/build/rfid-attendance` inside WSL.

---

## Prerequisites

You need:

- **Windows 10/11** with WSL 2 enabled
- **Ubuntu 24.04** WSL distribution
- **STM32MP1 OpenSTLinux Weston SDK** (scarthgap) — free download, requires ST.com account

Everything else is installed automatically by the steps below.

---

## 1. Install WSL & Ubuntu

Open **PowerShell as Administrator**:

```powershell
wsl --install Ubuntu-24.04
```

After reboot (if prompted), create a build user:

```powershell
wsl -d Ubuntu-24.04 -u root -- bash -c "
    useradd -m -G sudo -s /bin/bash builder &&
    echo 'builder:builder' | chpasswd &&
    echo 'builder ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/builder
"
```

Install required packages:

```powershell
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    sudo apt update && sudo apt upgrade -y &&
    sudo apt install -y build-essential cmake git make wget
"
```

---

## 2. Download the STM32MP1 SDK

1. Go to **https://www.st.com/en/embedded-software/stm32mp1dev.html#get-software**
2. Click **"Get Software"** and log in (free registration)
3. Under the download section, find and download:

   ```
   SDK-x86_64-stm32mp1-openstlinux-6.6-yocto-scarthgap-mpu-*.tar.gz
   ```

   > The `x86_64` variant is for cross-compilation FROM a PC. The filename
   > includes a version suffix like `v25.06.11` or `v26.02.18` — any scarthgap
   > release works.

4. Save the file somewhere accessible from WSL (your Downloads folder works).

---

## 3. Install the SDK

```powershell
# Extract the tarball
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    mkdir -p /tmp/sdk && cd /tmp/sdk &&
    tar xzf /mnt/c/Users/\$USER/Downloads/SDK-x86_64-stm32mp1-*.tar.gz
"

# Find the installer script (prints the path — copy it)
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    ls /tmp/sdk/stm32mp1-*/sdk/st-image-weston-*x86_64-toolchain-*.sh
"

# Run the installer (replace the path with the one from above)
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    cd /tmp/sdk/stm32mp1-openstlinux-*-scarthgap-*/sdk &&
    chmod +x st-image-weston-*x86_64-toolchain-*.sh &&
    sudo mkdir -p /opt/st/stm32mp1 &&
    ./st-image-weston-*x86_64-toolchain-*.sh -d /opt/st/stm32mp1 -y
"
```

Verify it worked:

```powershell
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    source /opt/st/stm32mp1/*/environment-setup-* &&
    arm-ostl-linux-gnueabi-gcc --version
"
```

Expected output: `arm-ostl-linux-gnueabi-gcc (GCC) 13.x.x`

---

## 4. Configure the Backend URL

Edit `api_client.h` and change the IP to match your backend server:

```cpp
#define API_BASE_URL "http://<YOUR-SERVER-IP>:3000"
```

---

## 5. Build the Project

Copy your project into WSL and compile:

```powershell
# Adjust the Windows path below to wherever your v2.1.2 folder lives
$PROJECT = "C:\path\to\v2.1.2"

wsl -d Ubuntu-24.04 -u builder -- bash -c "
    rm -rf ~/project && mkdir -p ~/project &&
    cp -r /mnt/c/$(echo '$PROJECT' | sed 's|\\|/|g; s|C:||; s|^/||')* ~/project/
"

wsl -d Ubuntu-24.04 -u builder -- bash -c "
    source /opt/st/stm32mp1/*/environment-setup-* &&
    cd ~/project &&
    cmake -B build -DCMAKE_BUILD_TYPE=Release &&
    cmake --build build -j \$(nproc)
"
```

The binary is at `~/project/build/rfid-attendance` in WSL.

To copy it back to Windows:

```powershell
# From the v2.1.2 folder:
wsl -d Ubuntu-24.04 -u builder -- bash -c "cp ~/project/build/rfid-attendance /mnt/c/$(pwd | sed 's|\\|/|g; s|C:||; s|^/||' 2>/dev/null)/"
```

---

## 6. Deploy to the Board

Transfer the binary to the STM32MP157DK-2 via SCP:

```bash
# From WSL:
scp ~/project/build/rfid-attendance root@<board-ip>:/usr/local/bin/
ssh root@<board-ip> chmod +x /usr/local/bin/rfid-attendance

# Run it:
ssh root@<board-ip> /usr/local/bin/rfid-attendance
```

---

## Rebuilding After Changes

After editing source files, rebuild with:

```powershell
wsl -d Ubuntu-24.04 -u builder -- bash -c "
    cp -r /mnt/c/path/to/v2.1.2/* ~/project/ &&
    source /opt/st/stm32mp1/*/environment-setup-* &&
    cd ~/project &&
    cmake --build build -j \$(nproc)
"
```

---

## How the Build Works

The `CMakeLists.txt` handles all dependencies automatically:

| Dependency | How It's Resolved |
|---|---|
| **ImGui** (v1.91.9) | Fetched from GitHub via `FetchContent` |
| **GLFW** (3.4) | Fetched from GitHub, compiled with Wayland backend |
| **OpenGL ES 3.0** | Provided by SDK sysroot (`glesv2`, `egl`) |
| **libcurl** | Provided by SDK sysroot |
| **FreeType** | Provided by SDK sysroot |
| **Wayland** | Provided by SDK sysroot |

> No manual dependency installation needed. CMake fetches and cross-compiles
> everything from source.

Key compile flags defined by CMake:

| Flag | Purpose |
|---|---|
| `IMGUI_IMPL_OPENGL_ES3` | ImGui renders with GLES3 instead of desktop OpenGL |
| `GLFW_INCLUDE_ES3` | GLFW headers use `<GLES3/gl3.h>` |

---

## Network & API

The device calls these endpoints on your backend (default port 3000):

| Method | Endpoint | Used For |
|---|---|---|
| `GET` | `/health` | Connection check |
| `POST` | `/api/scan` | RFID card lookup |
| `POST` | `/api/clock_in_with_signature` | Clock-in submission |
| `POST` | `/api/attendance_last_30` | 30-day history |

The backend IP is configured in `api_client.h` line 13.

---

## Troubleshooting

### `arm-ostl-linux-gnueabi-gcc: not found`

You forgot to source the SDK environment. Run:
```bash
source /opt/st/stm32mp1/*/environment-setup-*
```

### CMake can't find git

```bash
sudo apt install -y git
```

### "API connection failed" on the board

- Check the IP in `api_client.h` matches your backend machine
- Ensure the backend is running: `docker compose up -d` or `npm run dev`
- Test from the board: `curl http://<backend-ip>:3000/health`
- Check Windows firewall allows port 3000

### Port 3000 already in use

```powershell
netstat -ano | findstr ":3000"
taskkill /PID <pid> /F
```

### Drive not visible in WSL

Non-C drives must be mounted manually:
```bash
sudo mkdir -p /mnt/f && sudo mount -t drvfs F: /mnt/f
```

### Binary runs but screen is black

Ensure the board is booted into the Weston image (not the bare-minimal image).
The application requires Wayland + OpenGL ES.

---

## Files

```
v2.1.2/
├── main.cpp              # Application entry point
├── api_client.h          # HTTP API client
├── rfid_reader.h         # RFID serial reader
├── touch_handler.h       # Touch input handler
├── ui_renderer.h         # ImGui UI rendering
├── CMakeLists.txt         # Build configuration
├── toolchain-stm32mp1.cmake  # Toolchain file (optional)
├── build.sh              # Build script (run inside WSL)
├── assets/fonts/         # Font files
└── certs/                # TLS certificates
```

---

## Target Specs

| | |
|---|---|
| Board | STM32MP157DK-2 / STM32MP157F-DK2 |
| CPU | ARM Cortex-A7 dual-core, NEON + VFPv4 |
| Kernel | Linux 6.6 (OpenSTLinux scarthgap) |
| Graphics | OpenGL ES 3.0 via Wayland / EGL |
| Display | 480x800 DSI touchscreen |
