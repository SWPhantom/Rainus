#!/bin/sh
# Build with the host's current UTC time, then flash one board in ISP mode.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$script_dir"
fun_root=${CH32FUN_ROOT:-../../ch32fun}
programmer="$fun_root/minichlink/minichlink"

if [ ! -f "$fun_root/ch32fun/ch32fun.mk" ]; then
    echo "Cannot find ch32fun at $fun_root; set CH32FUN_ROOT to its path." >&2
    exit 1
fi
make -C "$fun_root/minichlink" minichlink
echo "Checking for a CH32V203 in USB ISP bootloader mode..."
if info=$("$programmer" -C isp -i 2>&1); then
    printf '%s\n' "$info"
else
    printf '%s\n' "$info" >&2
    echo "Enter bootloader mode: hold BOOT, press and release RST, release BOOT." >&2
    exit 1
fi
case "$info" in
    *"Detected CH32V203"*) ;;
    *) echo "The programmer did not identify a CH32V203; not flashing." >&2; exit 1 ;;
esac

build_epoch=$(date -u +%s)
case "$build_epoch" in
    ''|*[!0-9]*) echo "Could not read the computer's Unix time." >&2; exit 1 ;;
esac
if [ "$build_epoch" -lt 1577836800 ] || [ "$build_epoch" -gt 4294967295 ]; then
    echo "Computer time is outside the supported 2020–2106 range." >&2
    exit 1
fi
printf 'Build time (UTC): %s (Unix %s)\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')" "$build_epoch"
echo "Check that the computer clock is correct; compiling..."
make build "CH32FUN_ROOT=$fun_root" "BUILD_UNIX_TIME=$build_epoch"
echo "Flashing Rainus firmware..."
"$programmer" -C isp -w rainus.bin flash -b
echo "Flash complete. The board should reboot; see README.md for USB serial."
