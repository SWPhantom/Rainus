# Rainus CH32V203 prototype

This firmware is for the MuseLab nanoCH32V203 v1.1. It is an experimental
logger: test the switch, timestamps, SD writes, sleep current and reliability
before deployment. The default build uses a GPIO interrupt and STOP mode;
the optional USB debug build stays awake. STOP mode and long-term reliability
still need verification on the actual board and card.

The onboard microSD socket uses SPI1: PA3 is chip select, PA5 clock, PA6 MISO,
and PA7 MOSI. PA0 is the sensor input, PA2 is the manual flush input, and the
blue LED is on PA15.
[Board maker's schematic and documentation](https://github.com/wuxx/nanoCH32V203).

## Connect and prepare

1. With power disconnected, connect a **voltage-free** reed switch between PA0
   and GND. The internal pull-up holds PA0 high while the switch is open; a
   closure pulls it low and triggers a falling-edge interrupt. Do not connect a
   powered sensor circuit to the pin.
2. Optionally connect a voltage-free service button between PA2 (A2) and GND.
   A brief closure saves pending events and adds a timestamped collection
   marker, even if no events are pending. It is not a rain tip.
3. Insert a spare FAT16- or FAT32-formatted microSD card. The driver supports
   SD v2+ SDSC/SDHC/SDXC cards in SPI mode; exFAT is not supported.
4. Power the board. Startup now opens the log and finds its end, which can take
   about 15 seconds for the large test file. Afterward it waits for input. For
   now a switch already held closed at startup is not guaranteed to be counted.

The handler accepts an edge immediately and ignores further falling edges for
100 ms; this is an
initial debounce choice, not a measurement of the real gauge. It filters
short bounce but can count bounce lasting longer than 100 ms, and merges real
closures closer than 100 ms. A closure held continuously produces no new
falling edges. Each pin has independent debounce state. Test with the actual
gauge and service button.

If the timestamp used for debouncing goes backwards while the logger is
running, it stops with an RTC error instead of treating the jump as a new
closure. That check cannot detect an RTC reset between boots.

The blue LED stays off during automatic event capture and SD writes. A manual
PA2 flush gives two brief success flashes after completion, including when the
buffer was already empty. Fatal errors repeat groups of error flashes.
To turn off these firmware-controlled flashes, set `RAINUS_LED_ENABLED` to `0`
near the top of `rainus.c`, then rebuild and flash. Set it to `1` to restore
them. This controls the PA15 blue LED; it cannot turn off any power indicator
that is wired directly to a power rail. With flashes disabled, fatal errors
still stop the logger but have no visible indication. The awake USB debug build
can report them over serial. The fatal-error loop stays awake even with flashes
disabled, so its power draw is not representative of normal STOP-mode idle.

## Build and flash on macOS or Linux

The commands below assume a directory layout like this, and that the shell is
in `Rainus/ch32v203` unless stated otherwise:

```text
projects/
  Rainus/ch32v203/
  ch32fun/
```

Clone [ch32fun](https://github.com/cnlohr/ch32fun) beside Rainus, and install a
bare-metal RISC-V GCC toolchain, `make`, a native C compiler, and libusb. The
checkout used for the successful macOS build was at commit
`b28d285460ce052a179dba8f4a22c6f73ff8e328`; use that commit to reproduce this
setup, or try a newer version as an update. The [ch32fun installation
guide](https://github.com/cnlohr/ch32fun/wiki/Installation) has setup guidance
for macOS and other Linux distributions.

For a fresh checkout, from `Rainus/ch32v203`:

```sh
git clone https://github.com/cnlohr/ch32fun.git ../../ch32fun
git -C ../../ch32fun checkout --detach b28d285460ce052a179dba8f4a22c6f73ff8e328
```

Skip the clone if that directory already exists. Checking out the commit in an
existing ch32fun checkout changes its working version; keep any local work safe.

For example, on Debian or Ubuntu, install the tools with:

```sh
sudo apt update
sudo apt install git build-essential gcc-riscv64-unknown-elf libnewlib-dev libusb-1.0-0-dev libudev-dev usbutils screen
```

If you already have a working ch32fun toolchain and `minichlink`, you can reuse
them. Build the firmware and run the computer-side tests:

```sh
make -B build
make test
make -C ../../ch32fun/minichlink minichlink
```

`make -B build` creates `rainus.bin` without flashing. The last command builds
the host-native USB programmer on the computer you're using; don't copy a macOS
`minichlink` executable onto Linux. If ch32fun is elsewhere, pass its path to
make, for example `make build CH32FUN_ROOT=/path/to/ch32fun`, and use that
checkout's `minichlink` path in the commands below.

Automatic batches default to ten events. Override the length for a build or
host test with `make -B build LOG_BUFFER_LENGTH=64` or
`make test LOG_BUFFER_LENGTH=64`. For the one-step deployment script:

```sh
LOG_BUFFER_LENGTH=64 ./deploy.sh
```

The value must be a positive integer and the arrays must fit available RAM.
Ten remains the default pending the measurements below.

Run `make clean` in `Rainus/ch32v203` to remove the generated firmware files,
linker script and local test executables. It leaves source files and the
separately built `minichlink` programmer intact.

### Standby power-floor experiment

`power_floor/` is a separate diagnostic firmware for measuring the board
with the MCU in its lowest-power standby configuration. **It is not a logger:**
it disables the RTC and external wake inputs, does not initialize the SD card,
and does not record events. After one 100 ms startup flash it turns off the
LED and enters standby. RST restarts the test; BOOT/RST still enters the USB
bootloader. Time stops advancing while this test runs. Restore the normal
firmware with `./deploy.sh` from `Rainus/ch32v203` and verify the RTC time
before using the board as a logger.

To build and flash the experiment from `Rainus/ch32v203`:

```sh
make -C power_floor build
../../ch32fun/minichlink/minichlink -C isp -i
../../ch32fun/minichlink/minichlink -C isp -w power_floor/power_floor.bin flash -b
```

Enter USB1 bootloader mode before the last two commands and verify that the
programmer identifies the intended board. If ch32fun is elsewhere, pass
`CH32FUN_ROOT=/absolute/path/to/ch32fun` to make and use that checkout's
`minichlink` executable. As in the regular flashing instructions, disconnect
the PPK2 output and other power sources before connecting USB to Rainus.

With PPK2 Source Meter at 5 V connected to the board's verified 5V/GND pins,
measure settled current first without an SD card, then with the card inserted.
Power down before changing cards. These measure the complete board, not the MCU
alone. The card-present experiment leaves the card uninitialized and MCU GPIOs
high impedance: its current and pin states can differ from the normal logger,
so subtracting the two readings does not isolate the logger's SD-idle current.

### Flash over USB

1. Close any serial monitor connected to the board. Connect a data-capable
   USB-C cable to **USB1**, the board's USB device/programming connector.
2. Enter the bootloader: hold **BOOT**, press and release **RST**, then release
   **BOOT**. The bootloader is a different USB device from the running
   firmware, so the serial port may disappear at this point.
3. With only the intended ISP-mode board connected, build and flash it using
   the computer's current UTC time as the firmware's clock floor:

   ```sh
   ./deploy.sh
   ```

   The script checks that the bootloader identifies a CH32V203, obtains the
   host time, rebuilds, flashes, and requests a reboot. Keep the computer's
   clock synchronized. It prints the build time so you can check it. Build and
   flashing take time, so a newly initialized RTC will start slightly behind.
   If ch32fun is elsewhere, use `CH32FUN_ROOT=/path/to/ch32fun ./deploy.sh`.

   Alternatively, from `Rainus/ch32v203`, identify the board and flash a
   previously built binary explicitly:

   ```sh
   ../../ch32fun/minichlink/minichlink -C isp -i
   ../../ch32fun/minichlink/minichlink -C isp -w rainus.bin flash -b
   ```

   The first command should report a CH32V203. The second writes the firmware
   and requests a reboot. **A previously built binary retains its old build
   time**; use `./deploy.sh` for a fresh clock floor. Allow a few seconds for
   the application to start; the field build does not provide a serial port.

On Linux, `lsusb` can help identify the device in bootloader mode. The local
`minichlink` source recognizes bootloader IDs `1a86:55e0` and `4348:55e0`.
If it is listed but `minichlink` cannot access it, install ch32fun's udev rules
as described in its [minichlink README](https://github.com/cnlohr/ch32fun/blob/master/minichlink/README.md):

```sh
sudo make -C ../../ch32fun/minichlink install_udev_rules
```

This installs ch32fun's rules for several supported programmers, not just this
board. Check the resulting USB device's group and your membership with `groups`;
on many Debian/Ubuntu systems the supplied rules use `dialout`. If needed, add
your user to that group with `sudo usermod -aG dialout "$USER"`, then log out
and back in for the membership to take effect. Reconnect the board, put it back
in bootloader mode, and retry. Group names and permission policies can differ
by distribution.

## How events, sleep and writes work

Startup runs once after reset. The deployment loop then repeats:

```text
wait for a tip or manual-flush request (STOP while idle in the field build)
for a tip: add its timestamp; write and clear when LOG_BUFFER_LENGTH is reached
for a manual flush: write pending tips + collection marker, sync, clear, acknowledge
```

`wait_for_event()` handles the queue and faults; `sleep_until_switch_event()`
contains the documented hardware sleep sequence. For the reasoning behind
RTC waits, WFE, buffering and the remaining reliability issues, see the
[firmware review](CODE_REVIEW.md).

In the field build, after initialization the CPU enters **STOP mode** whenever
its event queue is empty. The RTC's 32.768 kHz crystal keeps time. A PA0 or PA2
falling edge wakes the CPU and triggers its interrupt handler, which reads the
RTC, debounces that input and queues a tip or flush marker. The main loop
processes the event, then returns to
STOP when there is no queued work. There is no periodic
USB polling in this build. The SD card and board regulator remain powered: STOP
mode does **not** mean that the RTC is the only component drawing power. A
power-profiler measurement of the complete board is required to know the actual
consumption and confirm that STOP mode is working as expected.

The handler accepts the first falling edge and ignores further edges less than
100 ms after the last accepted edge. This filters switch bounce, but also
discards any real closures that occur close together. If an RTC reading goes
backwards relative to the last accepted edge, the firmware reports an RTC
error and stops; it does not silently accept a questionable timestamp.

For reading the code, start with `main()` in `rainus.c`, then follow the calls:

| Source | Responsibility |
| --- | --- |
| `rainus.c` | Startup, input interrupt handlers, sleep, batching and error policy |
| `event.c` / `event.h` | Contact-bounce filter and first-in, first-out input queue |
| `rtc.c` / `rtc.h` | Initialize/read the Real-Time Clock, including fractional seconds |
| `chip_id.c` / `chip_id.h` | Format the chip's factory ID bytes as text |
| `log.c` / `log.h` | CSV rows, collection markers and one sync per saved batch |
| `sd_spi.c` | Move 512-byte sectors over Serial Peripheral Interface (SPI) wires |
| `funconfig.h` | Select the field/debug processor clocks |
| `usb_config.h` | Describe the optional serial device to the computer |
| `fatfs/ffconf.h` | Rainus's settings for the filesystem library |

The bundled `fatfs/ff.c`, `ff.h` and `diskio.h` are upstream library code.
External ch32fun supplies chip startup and hardware register names; its USB
library is linked only for `DEBUG_USB=1`. Those libraries require names such as
`disk_read`, `HandleUSBInput` and `EXTI0_IRQHandler`. The Rainus-owned files
explain the hardware terminology next to the code that uses it. Archived
`experiments/` and the separate `power_floor/` program are not part of the
normal logger build.

Both input interrupts stay enabled during SD writes. The queue can
hold 31 events (tips and flush markers) while the main loop is busy. If it overflows, at least one
event was lost and the firmware stops with four repeating error flashes (when
flashes are enabled). This queue does not prove all hardware edges are captured: closely spaced edges can
coalesce while an interrupt is pending, and switch bounce is filtered.

After **LOG_BUFFER_LENGTH tips (default 10)**, the firmware appends the batch
to `RAINLOG.TXT` and calls `f_sync()`. Startup opens the file after mounting and
finds its end. The file stays open across STOP sleep, retaining its byte position
and current cluster in RAM. Data batches avoid reopening, directory lookup and
traversal from the first cluster. The initial open/create can still be slow with
a large file or directory; that work now happens during startup after each
reset. Both inputs can queue events while SD preparation is busy. The card
remains powered.

Until a batch is saved, its events exist only in RAM. Power loss or reset can lose
up to LOG_BUFFER_LENGTH minus one buffered events during idle, and an unfinished batch is not flushed
automatically after a period of inactivity. A power failure during a write can
lose the current batch and possibly corrupt earlier FAT filesystem data too.
A write failure triggers the repeating five-flash error pattern when LED
indications are enabled. Automatic writes have no LED indication.

### Manually saving a partial batch

Briefly connect **PA2/A2 to GND**, then release it. The request is processed
after earlier queued tips. Their timestamps and the pending batch are saved,
followed by a collection-marker row of the form **`,<button Unix time>,`**.
The ID and build-time columns are empty; the middle column contains the RTC
time captured at the accepted button press, not the later SD-write time.
An empty batch still writes a marker. The rows and marker share one `f_sync()`;
the count clears only after it succeeds. Two 75 ms blue flashes acknowledge
completion, unless `RAINUS_LED_ENABLED=0`. Error flashes repeat instead of a
success acknowledgment on failure.

The logger continues operating after this action. A held service button does
not repeatedly flush. Tips queued after the marker go into the next batch,
including tips captured during the write or acknowledgment flashes. Thus the
acknowledgment covers events ahead of that request, not every event that might
arrive later. To collect the card, disconnect power after acknowledgment before
removing it. With LEDs disabled there is no visual success indication.

To use a marker to assess RTC drift, note the **trusted UTC time** when pressing
A2. Later compare it with the marker's recorded RTC time. The marker alone
does not supply a true reference or adjust the clock. Ignore rows with an empty
`chip_id` when counting rainfall/drip events; USB's `saved=` count also excludes
markers.

For a bench check at the default length, start with a new log, wait for startup
to finish, then generate one PA0 tip and press PA2: expect the header, one
sensor row followed by one marker, and two success flashes. Repeat with nine
tips: expect nine sensor rows followed by a marker. Press PA2 again with no new
tips: expect another marker and acknowledgment. Also check a held PA2 contact,
a request during an automatic write, and subsequent normal logging. Verify the
card contents after powering off, and measure the idle current after release.

The logging module also exposes `log_close()` for explicit cleanup and tests;
the deployment loop does not call it. Code must close the log before remounting
the card or opening the file through another handle. Do not remove or swap the
card during a run.

### Comparing startup and retained-cursor writes

Use the same card, supply and ten-event batches for each comparison. Measure
startup separately from at least three data batches, using:

- A new or empty `RAINLOG.TXT`.
- A large existing `RAINLOG.TXT`, such as the 6.3-million-record test file.
- A new `RAINLOG.TXT` in the directory containing the 6,300 test files.

Record duration and charge with the PPK2, then power off and confirm the records
on the card. The expected behavior is a potentially long startup, followed by
normal-duration data batches, including the first. Check idle current after
writes as well. Automatic sensor rows and SPI speed are unchanged; manual
collection requests additionally write a marker.

`make test` checks the CSV/header and sync behavior on FAT16/FAT32, including
sector/cluster crossings, explicit startup open and close/reopen, empty/partial
batches, timestamped manual markers (including empty batches), one sync per
manual request, and write/sync failures. Independent debounce and event-queue tests cover
flush-marker ordering, wraparound and overflow. The mock disk counts reads for
small and 3 MiB files to check that traversal happens during open, not data
batches. These counts are not hardware timings.

### Choosing a batch length

Compare these sizes with the same card, supply, SPI speed and file layout:

| LOG_BUFFER_LENGTH | Timestamp RAM | CSV bytes per full batch* | Fill time at one tip / 5 s |
| --- | ---: | ---: | ---: |
| 10 | 40 bytes | 480 | 50 s |
| 32 | 128 bytes | 1,536 | 2 min 40 s |
| 64 | 256 bytes | 3,072 | 5 min 20 s |
| 128 | 512 bytes | 6,144 | 10 min 40 s |

*For the current 24-character ID and ten-digit timestamps, excluding the header.
The starting file offset is not necessarily aligned to a sector boundary.

The writer calls `f_write()` once per formatted line, then syncs once per batch.
FatFs caches those calls in a 512-byte sector buffer, so this is not necessarily
a physical write per line. The driver still transfers sectors individually.
Manual flushes write the actual partial count, independent of the configured
automatic batch length.

Measure several steady-state batches per size, recording their duration,
charge, record count and resting idle current. Compare incremental charge per
record as `(batch charge - idle current * batch duration) / record count`.
Startup and manual-flush acknowledgment flashes should be outside that
measurement. Larger batches transfer more bytes, so the measured 3.26 mC for
ten records must not be treated as a constant cost for every batch size. Keep
the default at ten until the charge-per-record measurements show a useful gain.

## Clock and records

Each build embeds the host computer's current UTC Unix time. At startup the
RTC is initialized and set to the build time if newly started, or advanced to
that time if it is behind; a running clock already ahead is left alone. If RTC
initialization fails, the logger stops before enabling switch capture. A switch
closure while the RTC is starting is not recorded. Build and flash take time,
so this is approximate.
A bad host clock can put it too far ahead. If all RTC power is lost months
after flashing, the device can restart at the old build time and produce
plausible-looking but incorrect timestamps. The log includes the build time,
but does not indicate whether a fallback actually occurred. Verify the clock
at deployment and retrieval, and plan for this failure before trusting data.
The firmware no longer reads `SETTIME.TXT`.

The MCU RTC's 32-bit counter is split across two 16-bit registers. The code
checks that the high half didn't change during the read, because the low half
wraps every 65,536 seconds (about 18.2 hours). This prevents a rare incorrect
read during a two-year deployment. The Unix counter itself wraps in 2106.

New or empty log files get a header. Sensor records contain the factory chip
ID, event time and firmware build time, both in Unix seconds (UTC). Manual
collection adds a row with only its middle (RTC event-time) column populated:

```text
chip_id,event_unix_utc,build_unix_utc
CDAB207B0FBCFCE239E339E3,1794274153,1794274141
,1794274160,
```

The 24 hexadecimal characters are the chip's fixed 12-byte ID, read directly
from its UID registers; flashing from another computer does not assign the ID.
They identify the microcontroller, not the box or gauge. The build time is the
host clock when the firmware was compiled; it is not the deployment time and
does not prove the RTC was initialized from that build. Convert Unix seconds to
a human-readable date during analysis. For example, to inspect the sample
event time on a computer:

```sh
# macOS
date -u -r 1794274153
# Linux
date -u -d '@1794274153'
```

Older versions used other columns. Existing nonempty `RAINLOG.TXT` files are
left intact and do not get a new header; appending to one will mix formats.
Archive or rename the old file while the board is powered off to start a new,
self-describing log. Startup opens/creates the file without writing its CSV
header; the header appears with the first sensor batch or collection marker.

The board schematic shows no separate backup battery on VBAT. Do not assume
that the RTC will retain time when power is removed. The MCU's VBAT range is
1.8–3.6 V; a charged 18650 must not be connected directly to VBAT or 3.3 V.
See the [power and enclosure notes](POWER_AND_ENCLOSURE.md).

## How the SD driver communicates

The layers are `log.c` (CSV) → FatFs (files/directories) → `sd_spi.c` (sectors
and wire protocol) → card. SPI is Serial Peripheral Interface: one clock wire,
one data wire in each direction, and a chip-select wire. Each byte exchange
generates eight clock pulses, sending and receiving a bit per pulse.

`sd_begin_transaction()` selects our one socket by driving PA3 chip select
LOW (near zero volts), then waits for the card to finish any internal work.
`sd_end_transaction()` drives it HIGH (near 3.3 V) and sends eight final clock
pulses so the card releases its output wire. Neither function removes power.
`spi_send_and_receive_byte()` waits for hardware buffer flags, sends a byte,
and returns the received byte. Even reads must send something to generate
clock pulses; an all-ones byte (`0xFF`) supplies them. Input interrupts remain
enabled during this work.

`sd_send_command()` sends an SD command packet and returns the first response
byte (called R1 by the SD specification), or `SD_NO_RESPONSE`. Its named
constants describe what the commands do: for example,
`SD_COMMAND_READ_POWER_AND_ADDRESSING` sends CMD58, which reads the card's
Operating Conditions Register (OCR). CRC means Cyclic Redundancy Check, a
transmission checksum. Source comments explain the packet fields, response
sizes, start bytes, address units and checksum bytes before they are used.
Busy checks include 1 ms delays and bounded retries; transfer and interrupt
time makes these approximate limits rather than exact elapsed-time deadlines.

The driver keeps the tested startup delay, CMD0 readiness exception and reset
retries that resolved the SanDisk cold-start failure. Card-operation failures
propagate through FatFs to error 3 during mount or error 5 during file access.
The byte-level TXE/RXNE waits remain unbounded; bounding them is a separate
behavior change. GPIO event capture continues during normal SD waits, although
the foreground logger is occupied by the operation.

## USB diagnostics (optional build)

The default field build has **no application USB serial port**, whether awake
or asleep. The USB bootloader is separate and remains available using the
BOOT/RST sequence. To build and flash the diagnostic version instead:

```sh
DEBUG_USB=1 ./deploy.sh
```

That version runs at 48 MHz, keeps USB serial available, and stays awake. The
serial commands in this refactor still need a successful bench check. It
uses the same interrupt, queue and batching logic, but is not representative of
field power consumption or STOP-mode operation. Return to the field version by
running `./deploy.sh` without `DEBUG_USB=1`.

Open the USB1 serial port after the debug firmware starts. On macOS, find it
with `ls /dev/cu.*` and open, for example, `screen /dev/cu.usbmodem0071 115200`.
On Linux, look in `/dev/serial/by-id/` or `/dev/ttyACM*`, then open, for
example, `screen /dev/ttyACM0 115200`. The device name varies. Linux users may
need serial-port group access as described in the flashing instructions. Type
`?`, `s`, or `i` for a line containing the chip ID, error code, saved count,
current batch count, RTC error code, interrupt count, PA0 level and RTC Unix seconds. RTC error codes are
listed by name in `clock_result_t` in `rtc.h` (zero means success). Exit `screen` with Control-A, then K,
and close it before flashing. The serial port is absent in bootloader mode.

## Errors and verification

A fatal error repeats groups of blue LED flashes:

| Flashes | Error |
| --- | --- |
| 2 | RTC initialization or clock error |
| 3 | SD initialization or filesystem mount |
| 4 | Event queue overflow; at least one event was lost |
| 5 | SD file open, write or sync failed |

The logger disables further switch interrupts on an error and stops until
reset. Error flashes block; USB diagnostics run between groups in the debug
build. The error loop itself consumes awake power. Power off before removing
the card.

Before deployment, compare a known independent closure count with the logged
count at realistic and rapid rates, including held closures, bounce, and events
during a deliberately slow SD write. Verify timestamps across RTC rollovers,
reset and total power loss. Test multiple cards, full and missing cards,
unexpected power loss, and FAT recovery. Use the Nordic PPK2 to profile the
whole board in STOP and while writing; no two-year lifetime claim is established.

The `fatfs` directory contains FatFs R0.15 w/patch2, with its copyright and
redistribution notice retained in the source. Its configuration is specific
to this prototype.
