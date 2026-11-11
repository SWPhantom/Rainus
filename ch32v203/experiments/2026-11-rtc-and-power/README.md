# RTC and power experiments, November 2026

These are snapshots of the temporary programs used during the bench tests.
They are **diagnostics, not deployment firmware**: some stop the RTC and do
not log; the WFI variant did not wake on the sensor. Flashing any of them
replaces the current logger. Restore the normal firmware with `deploy.sh`
afterward. The RTC has no independent backup supply in this setup, so a power
cycle can make the time restart from the build time.

The full test history and measurements are summarized in the root `devlog.md`
and `ch32v203/POWER_AND_ENCLOSURE.md`. Measurements were at the board's 5 V
input with a Nordic PPK2; all card comparisons used the same Gigastone 32 GB
microSD. Never connect the PPK2 output and USB power simultaneously.

| Source | Purpose and observed result |
| --- | --- |
| `rtc_diagnostic_main.c` | Logger with extra RTC startup error codes. Use with one of the RTC files below. Eight flashes meant register synchronization timed out. |
| `rtc_combined.c` | Reconstructed original: RTC source and enable in one write, 1 s sync timeout; cold-start failure. |
| `rtc_combined_10s.c` | Reconstructed longer timeout experiment; still failed cold start. |
| `rtc_ordered.c` | Source and enable written separately, 1 s timeout; cold starts worked in the bench test. |
| `power_floor_uninitialized.c` | No RTC, no SD initialization, minimal standby: 32 µA without card, ~1 mA with card. Also available as `../../power_floor/power_floor.c`. |
| `sd_initialized_standby.c` | Initialize SD then enter standby with pins analog, RTC off: 1.14 mA. |
| `sd_initialized_stop.c` | As above but STOP instead of standby: 1.10 mA. |
| `sd_initialized_stop_logger_pins.c` | STOP with logger-like pin configuration, RTC off: 1.09 mA. |
| `logger_wfi.c` | Logger using WFI: 1.07 mA, but PA0 closure did not wake it. |
| `logger_wfe.c` | Logger with revised WFE event clearing and pending-edge check: 1.05 mA and PA0 wake worked. The equivalent change is in the normal logger. |

The two combined-write RTC files were reconstructed because the temporary
file was overwritten during the investigation: `rtc_combined.c` is the
pre-fix committed version; `rtc_combined_10s.c` changes its sync timeout from
1 s to 10 s. The other C files were copied from the actual experimental
working directories. Comments in the initialized-card standby source were
clarified after it was flashed; the executable code was unchanged.

## Rebuilding in an isolated directory

The experiments relied on the supporting project files and FatFs as they
were at Rainus commit `eed6bb17d424781539d85f2d7074ee0dca142078`, with
ch32fun at `b28d285460ce052a179dba8f4a22c6f73ff8e328`. Starting from that
baseline avoids silently picking up later changes. From the Rainus repository
root, set up a temporary copy (adjust `CH32FUN_ROOT` for your machine):

```sh
EXPERIMENTS="$PWD/ch32v203/experiments/2026-11-rtc-and-power"
CH32FUN_ROOT="$PWD/../ch32fun"
REPRO_DIR="$(mktemp -d)"
git archive eed6bb17d424781539d85f2d7074ee0dca142078 | tar -x -C "$REPRO_DIR"
```

For example, build the historical WFE experiment:

```sh
cp "$EXPERIMENTS/logger_wfe.c" "$REPRO_DIR/ch32v203/rainus.c"
make -C "$REPRO_DIR/ch32v203" -B build CH32FUN_ROOT="$CH32FUN_ROOT" BUILD_UNIX_TIME=1794373515
```

To build an RTC diagnostic instead, copy `rtc_diagnostic_main.c` to
`$REPRO_DIR/ch32v203/rainus.c` and one of the three `rtc_*.c` variants to
`$REPRO_DIR/ch32v203/rtc.c`, then run the same `make` command. The temporary
main maps RTC result values to 5 plus the result: 6 wrong source, 7 crystal
timeout, 8 synchronization timeout, and 9 write timeout.

For a minimal power test, choose one of the four power-test C files and its
Makefile (`basic` for `power_floor_uninitialized.c`, `sd` for the three
initialized-card tests):

```sh
mkdir -p "$REPRO_DIR/ch32v203/power_floor"
cp "$EXPERIMENTS/sd_initialized_stop.c" "$REPRO_DIR/ch32v203/power_floor/power_floor.c"
cp "$EXPERIMENTS/power_floor_sd.mk" "$REPRO_DIR/ch32v203/power_floor/Makefile"
cp "$EXPERIMENTS/power_floor_funconfig.h" "$REPRO_DIR/ch32v203/power_floor/funconfig.h"
make -C "$REPRO_DIR/ch32v203/power_floor" -B build CH32FUN_ROOT="$CH32FUN_ROOT"
```

These commands build but do not flash anything. The build timestamp in the
logger example is pinned to the original test; a freshly built logger should
use a fresh UTC build time. Power tests blink twice after successful SD
initialization and repeat three flashes on initialization failure; the
uninitialized standby test flashes once at startup. Always power off before
inserting or removing the card.
