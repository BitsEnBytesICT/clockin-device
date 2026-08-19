#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
base_dtb="${1:-$script_dir/build/stm32mp157f-dk2-stock.dtb}"
build_dir="${2:-$script_dir/build}"
overlay_source="$script_dir/stm32mp157f-dk2-clockin-overlay.dts"
overlay_blob="$build_dir/stm32mp157f-dk2-clockin-overlay.dtbo"
output_dtb="$build_dir/stm32mp157f-dk2-clockin.dtb"

if [[ ! -f "$base_dtb" ]]; then
    echo "Stock DTB not found: $base_dtb" >&2
    echo "Copy /boot/stm32mp157f-dk2.dtb from the board to that path first." >&2
    exit 1
fi

mkdir -p "$build_dir"
dtc -@ -I dts -O dtb -o "$overlay_blob" "$overlay_source"
fdtoverlay -i "$base_dtb" -o "$output_dtb" "$overlay_blob"

[[ "$(fdtget -t s "$output_dtb" /soc/bus@5c007000/spi@44009000 status)" == "okay" ]]
[[ "$(fdtget -t s "$output_dtb" /soc/bus@5c007000/timer@44007000 status)" == "okay" ]]
[[ "$(fdtget -t s "$output_dtb" /soc/bus@5c007000/timer@44007000/pwm status)" == "okay" ]]
[[ "$(fdtget -t s "$output_dtb" /soc/bus@5c007000/spi@44009000/rfid@0 compatible)" == "rohm,dh2228fv" ]]

echo "Built $output_dtb"
sha256sum "$base_dtb" "$overlay_blob" "$output_dtb"
