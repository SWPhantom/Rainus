# SD mount diagnostic

Temporary, **read-only** SD-card diagnostic used to investigate a 64 GB SanDisk
Extreme that caused the normal logger's three-flash SD-mount error. This is
not logger firmware: it does not initialize the RTC or capture switch events.
It does not open or write files, but it does initialize the card and read
filesystem sectors. The LED repeats a group of flashes:

| Flashes | Result |
| ---: | --- |
| 2 | FAT filesystem mounted successfully |
| 3 | CMD0 failed while entering SPI mode |
| 4 | CMD8 returned an unexpected response |
| 5 | CMD8 voltage/check-pattern response failed validation |
| 6 | CMD55 / ACMD41 card initialization failed or timed out |
| 7 | CMD58 operating-condition response failed |
| 8 | CMD16 block-length command failed on a byte-addressed card |
| 9 | A sector read failed during filesystem mount |
| 10 | No supported FAT filesystem was recognized |
| 11 | Another FatFs error occurred |

This diagnostic skips RTC setup, so startup timing differs from the normal
logger; its results must be interpreted with that difference in mind. It is
based on Rainus commit `b5bbe696038c78e154d9976a16dee29f601bb10c` and ch32fun
commit `b28d285460ce052a179dba8f4a22c6f73ff8e328`. The SD driver snapshot is
the normal driver with stage instrumentation added; its actual SD behavior
is otherwise unchanged.

The second variant, `sd_cmd0_diagnostic.c` with
`sd_spi_cmd0_diagnostic.c`, further splits the original three-flash CMD0
failure into: 3 = card select/ready check timed out before CMD0 was sent;
4 = CMD0 sent but no valid response; 5 = a valid-format response other than
the expected `0x01`. Its other results are shifted: 6 = CMD8 response,
7 = CMD8 echo, 8 = ACMD41, 9 = CMD58, 10 = CMD16, 11 = sector read,
12 = unsupported FAT volume, 13 = other FatFs error. Two still means success.
Like the first variant, it skips RTC initialization and does not write data.

The third variant pairs `sd_cmd0_diagnostic.c` with
`sd_spi_cmd0_no_ready.c`; it skips the ready wait *only* for CMD0. This tests
whether a card that has not entered SPI mode can respond to CMD0 even when
its data-output line does not satisfy the driver's previous ready check. It
uses the same blink codes as the second variant (3 is not reachable).

The fourth flashed variant pairs `sd_mount_diagnostic.c` with
`sd_spi_powerup_delay.c`, adding a 100 ms delay before the startup clocks but
otherwise keeping the original command sequence and initial diagnostic blink
codes. This tests whether additional time after power-up changes the result.
The separate `sd_cmd0_response.c` / `sd_spi_cmd0_response.c` pair was prepared
to identify particular CMD0 response values; the version without the added
delay was not flashed.

The fifth flashed variant pairs `sd_cmd0_diagnostic.c` with
`sd_spi_cmd0_delay.c`: it combines the 100 ms startup delay with skipping the
pre-CMD0 ready check. It uses the second variant's blink codes.

The sixth flashed variant pairs `sd_cmd0_response.c` with
`sd_spi_response_delay.c`. For a CMD0 failure, its counts identify the actual
response: 3 = 0x00, 4 = 0x05, 5 = 0x04, 6 = 0x7f, 7 = another valid-format
response, and 8 = no valid-format response. Two still means the mount succeeded;
counts for later initialization errors overlap and need to be interpreted using
the earlier experiments.

The seventh flashed variant pairs `sd_cmd0_diagnostic.c` with
`sd_spi_cmd0_retry.c`. It keeps the 100 ms delay and bypasses the pre-CMD0
ready check, then retries CMD0 up to 100 times, 10 ms apart. On the tested
SanDisk Extreme 64 GB, the user observed two flashes (successful mount) after
both a cold power-up and RESET. The normal logger was then updated separately;
the user reported no startup error flashes and confirmed that event records
were written to the SanDisk. Repeated cold starts, other cards, and longer
logging runs remain to be verified.

To rebuild in an isolated directory from the Rainus repository root:

```sh
EXPERIMENTS="$PWD/ch32v203/experiments/2026-11-sd-mount"
CH32FUN_ROOT="$PWD/../ch32fun"
REPRO_DIR="$(mktemp -d)"
git archive b5bbe696038c78e154d9976a16dee29f601bb10c | tar -x -C "$REPRO_DIR"
cp "$EXPERIMENTS/sd_mount_diagnostic.c" "$REPRO_DIR/ch32v203/rainus.c"
cp "$EXPERIMENTS/sd_spi_diagnostic.c" "$REPRO_DIR/ch32v203/sd_spi.c"
make -C "$REPRO_DIR/ch32v203" -B build CH32FUN_ROOT="$CH32FUN_ROOT"
```

For the second variant, use `sd_cmd0_diagnostic.c` and
`sd_spi_cmd0_diagnostic.c` in the two copy commands instead.

This only builds; it does not flash. Flashing replaces the normal logger.
After diagnosing, restore the normal firmware with `ch32v203/deploy.sh`.
Disconnect board power before swapping SD cards, and do not power the board
from USB and PPK2 simultaneously.
