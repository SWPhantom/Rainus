# Firmware review — 2026-11-16

Updated 2026-11-22: the logger now opens the file during startup, keeps it open
and syncs each batch. PA2 adds an ordered manual-flush request. Automatic batch
length is configurable, with ten remaining the default.

Updated 2026-11-23: the user reports that manual save works. The SD driver has
received a readability pass with descriptive helpers, named protocol values
and comments explaining hardware polling and its return values. Command
sequencing, explicit delays, retry budgets and error policy are preserved.

Updated 2026-11-25: an A2 collection request now appends a timestamped marker
after pending sensor rows, with a single sync. Empty requests also leave a
marker. Rainus-owned sources/configuration explain their data flow and hardware
terms for general programmers; SD helpers/constants use descriptive names.
The USB endpoint-direction comments are corrected without changing descriptors.

The main readability problem was that startup, interrupt/foreground sharing,
sleep entry and batching were all mixed into `main()`. The refactor puts the
hardware wait in `sleep_until_switch_event()` and queue/fault handling in
`wait_for_event()`. The deployment loop waits for a tip or flush request,
adds tip timestamps to the batch and writes when full or manually requested.
Initialization runs once per
reset; the event loop continues for the lifetime of that run.

## Decisions behind the code

**Chip ID.** C provides integer-to-hex formatting through `snprintf`, but no
single standard byte-array-to-hex function. The previous nibble lookup was a
normal implementation; it has been replaced with `%02X` for readability.
This runs once at startup. The output remains 24 uppercase characters in
ascending memory-address byte order, preserving the factory ID and zeroes.

**Debouncing and backwards time.** Normal RTC readings must increase. A
decreasing reading is a clock fault and already stops the logger with error 2;
it is not accepted as a sensor event. This check cannot discover a clock reset
between boots because the RAM debounce state starts fresh. The full Unix
seconds counter wraps in 2106, outside this deployment's intended lifetime.
The separate low 16-bit seconds register wraps about every 18.2 hours, so the
high/low/high read in `clock_seconds()` remains important even for short runs.

**RTC waits.** The RTC crystal and CPU peripheral bus are separate clock
domains. WCH's reference manual, sections 6.2.3 and 6.3.2, requires waiting for
register synchronization (`RSF`) and for configuration writes to finish
(`RTOFF`). `RTC->CTLRL` is a hardware control/status register, not ordinary RAM.
Timeouts bound a fault; removing them would change a reported error into a
possible permanent wait. They are not a recovery mechanism. Clock failures
lead to the terminal error loop; the awake USB build exposes the detailed
`clock_result_t` value through `rtc=`.

`clock_ticks()` combines Unix seconds and the fractional RTC divider into
32768-Hz ticks for the 100 ms bounce filter. CSV records still have whole
seconds. Its return value reports success/failure and its pointer argument
receives the time, only on success. This avoids confusing a timestamp with
an error value. The current code requests synchronization on every read,
including edges that occur while the CPU is already awake.

**Two RAM arrays.** The interrupt queue preserves arrival times while SD work
blocks the main loop. The batch groups timestamps to reduce the frequency
of writes. A write may take seconds: the measured 15-second operation overlaps
roughly three closures at a five-second interval. Blocking capture during that
operation would risk losing them. The two arrays are not alternating DMA
buffers and there is no task scheduler or application priority system. They
have different jobs. With the default length of ten, their payloads cost 296
bytes: 32 typed queue entries of 8 bytes and ten 4-byte timestamps. The queue
also carries manual-flush markers in order, avoiding a flag that could overtake
previously queued tips. The PA0 and PA2 handlers share the same interrupt
priority and cannot preempt one another; foreground pops mask interrupts.

**Sleep and assembly.** WFE means Wait For Event. On this QingKe core, the
instruction is spelled `wfi`; a control-register bit selects event behavior.
The code masks interrupts between checking for queued work and entering STOP
so an arriving edge cannot be serviced and then forgotten just before sleep.
The EXTI hardware event still wakes the CPU while interrupts are masked.
Interrupts are enabled again afterward to capture the timestamp.

The two-instruction sequence first consumes a software event, then waits for
a real one, unless an edge is already pending. The pending-edge check matters:
blindly replacing this with ch32fun's two-wait `__WFE()` helper would remove
it. The empty assembly statements are compiler barriers, emitting no CPU
instructions; they keep memory operations on the intended side of interrupt
masking. ch32fun's interrupt helpers do not have memory clobbers themselves.

**LED and USB.** Automatic logging has no LED flashes. A PA2 service request
gets two brief flashes after success, even if its batch was empty. These and
terminal-error flashes use blocking delays with input capture still enabled
for successful service requests. USB status work runs synchronously in foreground
code; the optional debug build stays awake so the console can remain usable.
Terminal errors disable both input interrupts. USB interrupts remain
available for diagnostics. Error blink numbers and automatic sensor rows are
preserved. Manual requests now add `,<press_unix_utc>,` after pending tips;
the press time comes from the interrupt queue. Buffer clearing and success
flashes happen only after the rows and marker sync. The USB saved-tip count
excludes markers. To measure drift, record trusted UTC at the press as well.

## Concrete limitations that remain

1. **Startup open can be expensive.** `log_open()` opens once after mounting
   and walks to the end. Data batches retain that cursor and sync without
   reopening. The user confirmed roughly 15 seconds for the first lazy-open
   batch followed by 500 ms for later batches on a 6.3-million-record file.
   That traversal now moves to startup; it is not eliminated. PA2 manual save
   has been reported working; startup timing, concurrent-input tests and idle
   current after these changes still need measurement. Automatic CSV rows are
   unchanged; manual collection markers are new.
2. **The interrupt is not guaranteed to be brief.** RTC synchronization uses
   1 ms polling delays and can wait up to 100 ms on a failure. Edges can
   coalesce in hardware while the handler is occupied. Reducing repeated
   synchronization requires checking the RTC read rules and measuring wake
   capture on the board; the queue alone cannot prove lossless capture.
3. **Some driver waits are still unbounded.** SPI's TXE/RXNE polling in
   `sd_spi.c` has no timeout. A peripheral fault can hang a write before it
   returns to the normal error handler. Adding RTC-style error bounds there
   would improve fault reporting; a full write can still contain many valid
   card operations, so one transfer timeout is not a total write deadline.
4. **Fault handling stops until reset.** It does not retry or save a fault
   record, and it consumes awake power. With LED indications off, failure is
   invisible in the field. A recovery/fault-record policy is a separate design
   decision, especially because retrying partial CSV writes can duplicate rows.
5. **Data durability and time remain deployment constraints.** RAM batches
   can be lost; power loss during an SD write can damage the filesystem. A
   cold start can reuse an old build timestamp without marking the fallback.
   The existing log format does not reveal every reset or every lost event.

## Validation

`make test` passed for independent debounce states, FIFO flush-marker ordering,
queue wraparound/overflow, ID byte order, startup open, empty/partial batches,
retained cursor and sync failures on FAT16/FAT32. The November 25 checks also
cover marker-only first saves, partial batches followed by markers, repeated
collection, logging after collection, exact row ordering, one sync per manual
request and failed writes/syncs. Host checks also passed with
batch lengths 1, 10, 32, 64 and 128. Field and USB debug builds passed for all
four benchmark sizes, plus LED-disabled builds; zero length is rejected.
The sleep function's operations match the previous version after flag renames;
both input lines remain in the check before the second wait. The SD driver
diff contains only comments and consistent identifier renames: numeric values
and operations match. RTC operations also match after helper/constant naming
changes, and USB descriptor/configuration bytes are unchanged. Host tests
cannot verify STOP current, wake races or peripheral clock
handshakes. Before deployment, repeat cold starts, compare a known closure
count (including closures during a long write), verify the SD contents and
measure idle current on the board. This refactor has not been flashed as part
of this review.

Sources: [WCH reference manual download](https://www.wch-ic.com/downloads/CH32FV2x_V3xRM_PDF.html),
[WCH-authored manual mirror, sections 6.2.3 and 6.3.2](https://atta.szlcsc.com/upload/public/pdf/source/20250114/3020F1FD898AACEC1298F6176560F8E0.pdf#page=73),
[QingKe V4 processor manual](https://www.wch-ic.com/downloads/QingKeV4_Processor_Manual_PDF.html),
[FatFs f_sync](https://elm-chan.org/fsw/ff/doc/sync.html).
