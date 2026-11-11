# Rainus CH32V203 prototype

This firmware is for the MuseLab nanoCH32V203 v1.1. It is an experimental
logger: test the switch, timestamps, SD writes, sleep current and reliability
before deployment. The default build uses a GPIO interrupt and STOP mode;
the optional USB debug build stays awake. STOP mode and long-term reliability
still need verification on the actual board and card.

The onboard microSD socket uses SPI1: PA3 is chip select, PA5 clock, PA6 MISO,
and PA7 MOSI. PA0 is the sensor input and the blue LED is on PA15.
[Board maker's schematic and documentation](https://github.com/wuxx/nanoCH32V203).

## Connect and prepare

1. With power disconnected, connect a **voltage-free** reed switch between PA0
   and GND. The internal pull-up holds PA0 high while the switch is open; a
   closure pulls it low and triggers a falling-edge interrupt. Do not connect a
   powered sensor circuit to the pin.
2. Insert a spare FAT16- or FAT32-formatted microSD/SDHC card. The driver
   supports SD version 2 cards; exFAT is not supported.
3. Power the board. After initialization it waits for a switch closure. For
   now a switch already held closed at startup is not guaranteed to be counted.

When LED flashes are enabled, each accepted edge flashes the blue LED once for
100 ms. The handler accepts an edge immediately and ignores further falling
edges for 100 ms; this is an
initial debounce choice, not a measurement of the real gauge. It filters
short bounce but can count bounce lasting longer than 100 ms, and merges real
closures closer than 100 ms. A closure held continuously produces no new
falling edges. Test with the actual gauge.

If the timestamp used for debouncing goes backwards while the logger is
running, it stops with an RTC error instead of treating the jump as a new
closure. That check cannot detect an RTC reset between boots.

To turn off all firmware-controlled flashes, set `RAINUS_LED_ENABLED` to `0`
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

Run `make clean` in `Rainus/ch32v203` to remove the generated firmware files,
linker script and local test executables. It leaves source files and the
separately built `minichlink` programmer intact.

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

In the field build, after initialization the CPU enters **STOP mode** whenever
its event queue is empty. The RTC's 32.768 kHz crystal keeps time. A PA0 falling
edge wakes the CPU and triggers the interrupt handler, which reads the RTC and
queues the event. The main loop optionally flashes the LED and adds the
timestamp to a batch, then returns to STOP when there is no queued work. There is no periodic
USB polling in this build. The SD card and board regulator remain powered: STOP
mode does **not** mean that the RTC is the only component drawing power. A
power-profiler measurement of the complete board is required to know the actual
consumption and confirm that STOP mode is working as expected.

The interrupt stays enabled during the LED flash and SD writes. The queue can
hold 31 timestamps while the main loop is busy. If it overflows, at least one
event was lost and the firmware stops with four repeating error flashes (when
flashes are enabled). This queue does not prove all hardware edges are captured: closely spaced edges can
coalesce while an interrupt is pending, and switch bounce is filtered.

After **10 events**, the firmware appends the batch to `RAINLOG.TXT` and closes
the file. Until then the events exist only in RAM. Power loss or reset can lose
up to nine buffered events during idle, and an unfinished batch is not flushed
automatically after a period of inactivity. A power failure during a write can
lose the current batch and possibly corrupt earlier FAT filesystem data too.
When enabled, the LED confirms an event was accepted into RAM, **not** that it
was saved to the SD card. No safe power-off or manual flush exists yet.

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

New or empty log files get a header. Each record contains the factory chip ID,
the event time and the firmware build time, both in Unix seconds (UTC):

```text
chip_id,event_unix_utc,build_unix_utc
CDAB207B0FBCFCE239E339E3,1794274153,1794274141
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
self-describing log. The header and events are written when the first batch of
ten is saved, not at startup.

The board schematic shows no separate backup battery on VBAT. Do not assume
that the RTC will retain time when power is removed. The MCU's VBAT range is
1.8–3.6 V; a charged 18650 must not be connected directly to VBAT or 3.3 V.
See the [power and enclosure notes](POWER_AND_ENCLOSURE.md).

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
| 5 | SD file open, write or close failed |

The logger stops processing events on an error. A repeated error flash pattern
itself consumes power. Power off before removing the card.

Before deployment, compare a known independent closure count with the logged
count at realistic and rapid rates, including held closures, bounce, and events
during a deliberately slow SD write. Verify timestamps across RTC rollovers,
reset and total power loss. Test multiple cards, full and missing cards,
unexpected power loss, and FAT recovery. Use the Nordic PPK2 to profile the
whole board in STOP and while writing; no two-year lifetime claim is established.

The `fatfs` directory contains FatFs R0.15 w/patch2, with its copyright and
redistribution notice retained in the source. Its configuration is specific
to this prototype.
