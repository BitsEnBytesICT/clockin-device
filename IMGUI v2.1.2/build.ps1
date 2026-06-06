# build.ps1 - Cross-compile v2.1.2 for STM32MP157DK-2 from Windows
# Copies project to WSL, sets up SDK (if needed), builds, and copies binary back

param(
    [string]$SdkTarball = ""  # Path to downloaded SDK tar.gz
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$WslDistro = "Ubuntu-24.04"
$WslProjectDir = "/home/builder/project"

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " STM32MP157DK-2 Cross-Compilation Build" -ForegroundColor Cyan
Write-Host " Project: v2.1.2 RFID Attendance System" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Copy project files to WSL
Write-Host "[1/5] Copying project files to WSL..." -ForegroundColor Yellow
wsl -d $WslDistro -u builder -- bash -c "rm -rf $WslProjectDir && mkdir -p $WslProjectDir"
Get-ChildItem -LiteralPath $ProjectDir -File | ForEach-Object {
    $src = $_.FullName
    $dest = ($_.FullName -replace '\\', '/') -replace '^C:', '/mnt/c'
    # Actually simpler: just copy via wsl
}
# Copy all files from v2.1.2 to WSL
wsl -d $WslDistro -u builder -- bash -c "cp -r /mnt/c/Users/derko/Desktop/test5/v2.1.2/* $WslProjectDir/"

# Step 2: Check/install SDK
Write-Host "[2/5] Checking SDK installation..." -ForegroundColor Yellow
$sdkInstalled = wsl -d $WslDistro -u builder -- bash -c "ls -d /opt/st/stm32mp1/*/ 2>/dev/null | head -1"
if (-not $sdkInstalled) {
    Write-Host "SDK not installed. Running setup..." -ForegroundColor Yellow
    
    if ($SdkTarball -and (Test-Path $SdkTarball)) {
        Write-Host "Copying SDK tarball to WSL..." -ForegroundColor Gray
        $tarballName = Split-Path $SdkTarball -Leaf
        $wslTarballPath = "/home/builder/$tarballName"
        wsl -d $WslDistro -u builder -- bash -c "cp /mnt/c/Users/derko/Desktop/test5/v2.1.2/$(Split-Path $SdkTarball -Leaf) $wslTarballPath 2>/dev/null || cp '$($SdkTarball -replace '\\','/')' $wslTarballPath 2>/dev/null"
    } else {
        Write-Host ""
        Write-Host "SDK NOT FOUND. Please download it first:" -ForegroundColor Red
        Write-Host "  1. Go to: https://www.st.com/en/embedded-software/stm32mp1dev.html#get-software" -ForegroundColor White
        Write-Host "  2. Click 'Get Software' (requires free ST.com account)" -ForegroundColor White
        Write-Host "  3. Download: SDK-x86_64-stm32mp1-openstlinux-6.6-yocto-scarthgap-mpu-v26.02.18.tar.gz" -ForegroundColor White
        Write-Host "  4. Re-run: .\build.ps1 -SdkTarball <path-to-tarball>" -ForegroundColor White
        Write-Host ""
        exit 1
    }
    
    Write-Host "Installing SDK (this may take a few minutes)..." -ForegroundColor Yellow
    wsl -d $WslDistro -u builder -- bash -c "chmod +x $WslProjectDir/setup_sdk.sh && $WslProjectDir/setup_sdk.sh"
}

# Step 3: Source environment and configure build
Write-Host "[3/5] Configuring CMake build..." -ForegroundColor Yellow
wsl -d $WslDistro -u builder -- bash -c @"
cd $WslProjectDir
chmod +x build.sh
./build.sh
"@

# Step 4: Copy binary back to Windows
Write-Host "[4/5] Copying binary to Windows..." -ForegroundColor Yellow
Copy-Item -Path "\\wsl$\Ubuntu-24.04\home\builder\project\build\rfid-attendance" -Destination "$ProjectDir\rfid-attendance" -Force -ErrorAction SilentlyContinue

if (Test-Path "$ProjectDir\rfid-attendance") {
    $binInfo = Get-Item "$ProjectDir\rfid-attendance"
    Write-Host "[5/5] BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host "Binary: $ProjectDir\rfid-attendance ($('{0:N0}' -f $binInfo.Length) bytes)" -ForegroundColor White
    Write-Host ""
    Write-Host "To deploy to STM32MP157DK-2:" -ForegroundColor Cyan
    Write-Host "  scp $ProjectDir\rfid-attendance root@<board-ip>:/usr/local/bin/" -ForegroundColor White
    Write-Host "  ssh root@<board-ip> 'chmod +x /usr/local/bin/rfid-attendance'" -ForegroundColor White
} else {
    Write-Host "Build may have failed. Check WSL output above for errors." -ForegroundColor Red
}
