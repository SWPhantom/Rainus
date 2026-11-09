# Rainus CH32V203 bench prototype

This is a **continuously running bench prototype** for the MuseLab
nanoCH32V203 v1.1. A board has been flashed from macOS, and its switch input,
LED, USB diagnostics, and SD log have been checked on the bench. The build,
flashing, and serial workflow has also been tried on Linux. The firmware is not
field-ready and does not yet implement sleep, automatic retries, event buffering
during SD writes, or backup power. Use a spare SD card and do not rely on it for
a deployment.

The v1.1 board schematic assigns the onboard microSD socket to SPI1: PA3 is
chip select, PA5 clock, PA6 MISO, and PA7 MOSI. The external input uses PA0.
The board's PA15 LED indicates a successfully written event or an error.
[Board maker's schematic and documentation](https://github.com/wuxx/nanoCH32V203).

## Connect and prepare

1. With power disconnected, connect a **voltage-free** reed switch between
   the board's PA0 and GND pins. The firmware enables PA0's internal pull-up.
   Do not connect the original powered Rainus terminal circuit to this input.
2. Insert a spare, FAT16- or FAT32-formatted microSD/SDHC card. The current SD
   driver supports SD version 2 cards; exFAT is not supported. The program
   creates/appends `RAINLOG.TXT`, and it may consume `SETTIME.TXT`.
3. Connect USB power. The prototype draws continuous current; don't use its
   current measurement to estimate the finished logger's battery life. The
   current diagnostic firmware runs at 48 MHz to support USB serial.

The prototype accepts a closure that remains closed for 20 consecutive samples
(nominally 20 ms), then requires a stable open interval before another event.
This is a starting point, not a measured reed-switch specification. A closure
already present at startup is counted once after the stable interval.

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
make build
make test
make -C ../../ch32fun/minichlink minichlink
```

`make build` creates `rainus.bin` without flashing. The last command builds
the host-native USB programmer on the computer you're using; don't copy a macOS
`minichlink` executable onto Linux. If ch32fun is elsewhere, pass its path to
make, for example `make build CH32FUN_ROOT=/path/to/ch32fun`, and use that
checkout's `minichlink` path in the commands below.

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
   USB to reconnect; press RST once if the application does not start.

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

## Set and check the clock

The onboard 32.768 kHz crystal drives the RTC. Each build embeds the computer's
Unix time in UTC. On startup, if the backup domain has no recognized time or
the RTC is earlier than the embedded time, the firmware sets the RTC forward to
that build time. It leaves an RTC that is already later unchanged. USB status
and each new log record identify whether this floor was applied (`BUILD`) or a
time was explicitly set (`SET`).

**Build time is an estimate, not proof of the current time.** It is behind by
the time taken to build, flash, and start. If the device loses all RTC power
months later, it may restart at that same old build time; `BUILD` and the build
timestamp in the log expose the fallback but cannot recover the lost time. An
incorrect computer clock can also advance a good RTC too far, and the floor
won't correct a clock that is already too far ahead. A recognized backup marker
does not prove the clock ran continuously or is accurate. Check the time against
a trusted clock at deployment and retrieval.

To explicitly set a different time, power off the board and create a plain-text
file called `SETTIME.TXT` in the root of the SD card, containing a decimal Unix
timestamp in UTC followed by a newline. It must be at least the embedded build
time, or the firmware reports a time-file error. Put the card in the powered-off
board, then power it up. The firmware deletes the file before setting the RTC so
it won't silently reuse an old setting after a later reset. The timestamp is
applied when the board reads the file, not when the file was created. For a
bench test, prepare a timestamp for a known near-future time and apply power at
that time; startup introduces a small additional delay. Verify the result against
a trusted clock. This is not yet a precise clock-synchronization interface.

Resetting the MCU while its RTC backup domain stays powered should preserve the
time, but this has **not** been verified on the board. Removing all power may
erase the time. The published v1.1 schematic shows the MCU's VBAT pin connected
to a decoupling capacitor, with no backup cell or VBAT header connection shown.
The MCU supports a backup supply, but using one on this board needs hardware
design and verification. Its VBAT operating range is 1.8–3.6 V; do not connect
a fully charged 18650 directly to VBAT or to the 3.3 V rail. Verify all supply,
switchover, and reverse-current limits in the [WCH datasheet](https://datasheet.lcsc.com/lcsc/2204251630_WCH-Jiangsu-Qin-Heng-CH32V203C8T6_C3001172.pdf)
before designing a backup supply.

## Read the results

Each accepted closure appends one line. The first four columns match the
earlier CH32 prototype; two new columns identify the time source and build
floor. Temperature and humidity are not recorded:

```text
00112233445566778899AABB,2026-11-09T03:00:00,1794193200,847508400,BUILD,1794193200
```

The first field is the chip's factory ID, represented by 24 hexadecimal
characters, including leading zeroes. Bytes are in ascending address order
from the CH32V203's three UID registers. It identifies the microcontroller,
not the enclosure or gauge; keep a mapping when assembling or replacing units.
Verify that IDs remain the same after reflashing and differ across the actual
boards before relying on them. Earlier records use the literal ID `BENCH`.

The remaining fields are UTC timestamp, Unix seconds, and seconds since
2000-01-01 UTC, time source (`BUILD` or `SET`), and the embedded build time as
Unix seconds. `BUILD` means the clock was last initialized or advanced to a
build-time floor; it does **not** certify that the time is accurate. `SET`
means an explicit time file was applied; it does not guarantee accuracy either.
Old records from previous firmware have four columns, so existing files can
contain both formats. The old analysis script expects temperature and humidity
in columns five and six and must be updated before analyzing the new format.
The event timestamp is captured at the start of the final closure candidate,
before the 20 ms confirmation. Two quick blue LED flashes follow a successful
write.

## USB diagnostics

When the firmware is **running**, it provides USB CDC serial over USB1, the same
connector used for flashing. It does not provide the serial console while the
board is in bootloader mode. After flashing, wait for it to reboot or press RST
without holding BOOT. Find the port, then open it in a serial monitor:

- **macOS:** run `ls /dev/cu.*` and look for `/dev/cu.usbmodem...`. On the
  tested Mac, the port was `/dev/cu.usbmodem0071`:

  ```sh
  screen /dev/cu.usbmodem0071 115200
  ```

- **Linux:** run `ls -l /dev/serial/by-id/` or `ls -l /dev/ttyACM*` and look
  for the Rainus USB serial device. The following is an example; the actual
  number can differ:

  ```sh
  screen /dev/ttyACM0 115200
  ```

  If opening the port gives a permission error, check the group shown by
  `ls -l /dev/ttyACM0` and your groups with `groups`. On many Debian/Ubuntu
  systems the serial-port group is `dialout`; see the group-membership steps
  above. Log out and back in after changing group membership. If no port appears,
  check the USB1 cable, make sure the firmware is running rather than the
  bootloader, and inspect `lsusb`; the current diagnostic firmware identifies
  itself as `1209:d035`.

Replace the example port with the one shown on your computer. The board prints
status about every two seconds; send `?` for another status line, or `i` to
request the chip ID. The baud
setting is conventional for USB CDC, not a physical UART baud rate. Messages
may be missed when the serial monitor is not open. To exit `screen`, press
Control-A, then K, and confirm. Close the monitor before trying to flash again.

Example: `err=0 rtc=99 fat=0 sd=99/0 logs=2 time=BUILD`. Here `err=0` means no fatal
error, `rtc=99` means RTC initialization completed, `fat=0` means the card's
filesystem mounted, and `sd=99/0` means SD initialization completed. `logs`
is the number of events successfully written since the current boot, and `time`
is the clock source described above. If an
error occurs, `err` matches the LED pulse count in the table below. For RTC errors, `rtc`
identifies the stage: 1 clock-source check, 2 crystal startup, 3 initial
synchronization, 4 entering configuration, 5 prescaler write, and 6 syncing an
already-running clock. The `sd` pair gives the last initialization command and
its response, for debugging rather than an end-user status code.

If a fatal error occurs, the LED repeats a group of pulses followed by a pause:

| Pulses | Error |
| --- | --- |
| 1 | RTC oscillator or initialization |
| 2 | SD initialization or filesystem mount |
| 3 | Invalid time file or failure applying it |
| 4 | Log open, write, or close failure |

The logger stops accepting events on an error; fix the cause and reset it.
Power off before removing the card. FAT writes can be corrupted by interruption,
even though the file is closed after each event.

## Important limits and next tests

Input sampling stops while an SD write is in progress. A second
closure in that interval can be missed; the current code does not count or
report such losses. The first physical tests should use a known number of
closures, including bounce, held closures, and two-second spacing, and compare
the result with an independent counter. Confirm the LED, RTC startup and reset
retention, time setting, FAT32 as well as FAT16 cards, and missing-card errors.
Then implement event capture during writes and sleep/wake, and use the power
profiler to measure the **whole board**, including card and regulator.
See [power and enclosure notes](POWER_AND_ENCLOSURE.md) for a measurement plan
and the current hardware constraints.

The `fatfs` directory contains FatFs R0.15 w/patch2 as found in the local
ESP-IDF checkout, with its copyright and redistribution notice retained in the
source. The configuration in `ffconf.h` is specific to this prototype.
