#!/bin/sh

for pwm in /sys/class/pwm/pwmchip*/pwm0; do
    [ -d "$pwm" ] || continue
    device="$(readlink -f "$(dirname "$pwm")/device" 2>/dev/null || true)"
    case "$device" in
        *44007000*) printf 0 > "$pwm/enable" 2>/dev/null || true ;;
    esac
done

exit 0
