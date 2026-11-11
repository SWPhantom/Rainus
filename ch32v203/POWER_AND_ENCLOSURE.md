# Power and enclosure investigation

These are design and test notes, not a field-ready power design. Numbers from a
chip or battery datasheet are not a measurement of the complete logger. The
default field firmware requests STOP mode between events, but this has not yet
been verified by current measurement. It leaves the SD card powered. The optional
USB diagnostic build keeps the MCU and USB awake.

## Board power and SD

The published [nanoCH32V203 v1.1 schematic](https://github.com/wuxx/nanoCH32V203/blob/master/hardware/nanoCH32V203.pdf)
shows both header pins numbered 19 connected to `5V_USB`, with ground at pin 20.
That input goes through a fuse to an ME6211C33 regulator, whose 3.3 V output
feeds the MCU and SD socket. Use board silkscreen and a multimeter to confirm
the pins on the actual board. A regulated 5 V supply connected to `5V_USB` and
GND supplies the same input rail as USB. Do not connect a battery or other 5 V
source there while a computer's USB cable is also connected unless a designed
power-path circuit prevents reverse current.

A protected single 18650 is about 4.2 V when fully charged; the `5V_USB` rail
feeds the board's 3.3 V regulator, so it is the plausible *input* to evaluate
for a direct-cell setup, not the 3.3 V pin. This is not yet a validated battery
design: check the actual board and protection circuit, supply sag during SD
writes, the low-voltage cutoff, and the entire discharge curve. As cell voltage
falls, the regulator eventually cannot maintain 3.3 V. A regulated 5 V boost
converter is another option, but it adds its own idle current and conversion
losses. Never connect USB and the battery supply simultaneously without proper
power-path isolation.

The MCU's VDD is its main supply; VBAT supplies its RTC backup domain when the
main power is absent. The schematic shows VBAT connected to a decoupling
capacitor, with no battery or header connected. With no board modifications,
there is no provided separate VBAT battery connection. See the [WCH
datasheet](https://datasheet.lcsc.com/lcsc/2204251630_WCH-Jiangsu-Qin-Heng-CH32V203C8T6_C3001172.pdf)
for operating and backup conditions. Do not attach a fully charged 18650
directly to the 3.3 V rail or VBAT.

The onboard SD socket's VDD is wired directly to the board's 3.3 V rail; the
schematic shows no switch that firmware can use to disconnect that socket
independently. The card is therefore powered whenever the rail is powered.
Actual powered-idle current depends on the card and its state and must be
measured. Shutting off the entire board also stops the MCU and, without an
independent backup, its RTC. An external switched SD module and isolation are
an option to evaluate, but that would use a separate socket and require careful
control of power sequencing and signal backfeed.

## First measurements with the Nordic PPK2

1. Use source-meter mode, set **5.0 V**, and connect PPK2 VOUT to the verified
   `5V_USB` header pin and PPK2 GND to board GND. Power the PPK2 from the
   computer. Disconnect the board's USB cable and any other board power source
   so PPK2 is the only source. Recheck polarity before turning PPK2 output on.
   This measures the whole board at the 5 V input, including the regulator,
   card and power indicator circuitry. USB serial will not be connected.
2. With the field firmware, record startup, several minutes without closures,
   and at least 20 individual closures (including both batch writes and LED
   flashes). Repeat with different cards. Record mean current, voltage, peak,
   and write duration; integrate the *extra* current above idle across a full
   batch to estimate charge per event. Don't infer card-only current from this
   measurement.
3. Compare multiple card types and measure after each card has settled for
   minutes, not only immediately after a write. The firmware enters an error
   loop without a card, even when LED flashes are disabled, so its no-card
   current is not comparable to normal idle.
4. Compare the known number of closures with the SD file, including closures
   during writes, then repeat at the highest expected site event rate. Record
   supply voltage at the board during writes as well; the PPK2 current trace
   alone does not establish that a battery can hold voltage under the peaks.

The separate `power_floor/` experiment enters MCU standby with the RTC, SD
initialization, RAM retention and sensor wake disabled. It can be measured with
and without a card, unlike the logger's no-card error loop. This is a board
power-floor investigation, not a functional logger or a measurement of the
normal initialized card's idle current. See the README for flashing, recovery,
and the important RTC and card-state limitations.

On November 10, 2026, a first PPK2 Source Meter test at 5 V measured about
**32 µA without a card** and **1 mA with a Gigastone 32 GB microSD card** in
this standby experiment. A separate temporary diagnostic initialized the same
card before entering the same standby configuration and measured **1.14 mA**;
its two startup flashes confirmed initialization completed. In both tests the
RTC was off and the GPIOs were set to analog input before standby. These are
whole-board measurements; card state, pin state, and measurement variation
must be considered before attributing the difference to the card alone. The
initialized-card test does not reproduce the logger's STOP-mode pin states.
With the same initialized-card diagnostic changed from standby to STOP with
the normal regulator, the user measured approximately **1.10 mA**. That
diagnostic still had the RTC and sensor wake disabled and GPIOs set to analog
input, so it is not the normal logger. The similar standby and STOP readings
do not by themselves identify the MCU's current or the card's independent
current, and do not explain the earlier approximately 1.96 mA logger reading.
A further STOP diagnostic kept the initialized SD card's SPI pins, PA0's
pull-up and the LED pin configured like the logger instead of setting GPIOs to
analog input; the measured whole-board current was approximately **1.09 mA**.
The RTC and sensor interrupts remained off. This suggests the tested pin
configuration alone did not account for the earlier higher logger reading.
The normal field logger, reflashed with the cold-start RTC fix and the same
card, subsequently measured approximately **1.95 mA** at idle; its LED stayed
off. Unlike the diagnostics, it enables the RTC and switch wake, mounts the
filesystem, and uses WFE to request STOP. At that point, the tests had not yet
isolated which difference accounted for the roughly 0.86 mA gap.
A subsequent temporary build of the logger changed only its sleep instruction
from WFE to WFI; under the same setup it measured approximately **1.07 mA**
at idle, with no repeating LED errors. This implicates the WFE path in the
extra draw, but the WFI build did not wake for a switch closure. A separate
WFE test consumed a software event before sleeping and checked for a pending
switch edge; it measured approximately **1.05 mA** idle, flashed on closures,
and returned to that level afterward. After this WFE sequence was added to the
normal logger and flashed, the user reported approximately **1.1 mA** idle
with the same card. It has not yet been subjected to a long-duration or
controlled rapid-event loss test. A roughly 28 mA peak was also observed
during the diagnostic, but its duration and relationship to an SD write were
not independently verified.

Use the [PPK2 source-meter instructions](https://docs.nordicsemi.com/r/bundle/ug_ppk2/page/ug/ppk/measure_current_source_meter.html)
and its current and accuracy ranges. The simple average-current approximation
is `I_average = I_idle + events_per_day * Q_event / 86400`, where `Q_event` is
the extra microcoulombs per event and currents are in microamps. Use measured
event frequencies and current profiles, and verify the intended STOP state by measuring it.
For battery choices at a different voltage, use energy (integral of V times I),
including converter efficiency and quiescent draw; do not compare 5 V input mAh
directly to battery mAh.

As an optimistic illustration, 200 mAh / one year is about 23 microamps
continuous before capacity derating, cell self-discharge, and conversion loss.
A coin cell also has limited pulse delivery; SD write peaks and supply sag can
be decisive even if the long-term average meets this budget. Measure the card,
consider storage and temperature, and test throughout discharge. A reservoir
capacitor may help short bursts, but its required size, leakage and recharge
behavior depend on the measured waveform and allowable voltage sag.

For context, one [ADTEC microSD datasheet](https://www.adtec.co.jp/file/industry/Datasheet_microSD_SLC_1.pdf)
lists 73 mA typical write current and 0.14 mA typical standby under its stated
conditions. Those are examples for that card, not measured Rainus numbers or a
guaranteed instantaneous peak. The [ME6211C33 regulator datasheet](https://datasheet.lcsc.com/lcsc/Nanjing-Micro-One-Elec-%E3%80%80%E3%80%80ME6211C33M5G-N_C82942.pdf)
lists about 60 µA typical supply current at its specified test conditions. The
board schematic identifies an ME6211C33M5G, but the fitted part and actual
current need verification. Regulator draw alone could consume a substantial
part of a two-year coin-cell budget when powered through this path.

### Two-year demanding case

At one closure every five seconds continuously, the logger would receive 17,280
events/day and about 12.6 million events in two 365-day years. That's a stress
case, not an assumption that real sites run at that rate continuously. The
current approximately 48-byte CSV records would occupy about 605 MB before
filesystem overhead. The 24-character chip ID is included in that estimate.

At the **battery terminals**, an ideal 200 mAh cell supplies an average of only
11.4 microamps over two years; an ideal 2,500 mAh cell supplies 143 microamps.
These figures include no derating or safety margin. The event-energy budget must
share that allowance with continuous idle current, leakage, and conversion loss.
For example, if each event used an extra 1 mC at the measured 5 V input, the
events alone would average 200 microamps at 5 V at one event per five seconds;
this is an example, not a measured value. Compare actual battery-side energy,
not currents on opposite sides of a voltage converter.

## Storage versus buffering

The firmware batches 10 event timestamps in RAM, while an interrupt-driven
queue holds up to 31 additional events during an SD write. Between successful
commits, a power loss can lose up to nine buffered events. During a write, it
can lose the current batch or corrupt earlier FAT data as well. An unfinished
batch remains in RAM indefinitely during a dry period. A safe service flush and
low-battery handling are not implemented. A queue overflow stops the logger
with an error (visible only when LED flashes are enabled), but may already have
lost events. Test capture under real write delays and realistic bounce before
relying on it.

Batching may save energy, but the actual card idle current and event energy
still need measuring. FRAM or another nonvolatile store remains an option for
reducing the loss window. The firmware opens the same file in append mode once
per batch. The bundled FatFs implementation walks the file's cluster chain to
find the end on open, so append latency and reads can grow as the log gets
larger. Test a large representative file and evaluate bounded-size files and a
safe recovery strategy before a long deployment.

## Oil and water

An intact reed switch's contacts are hermetically sealed; a nonmagnetic oil
outside the glass should not stop magnetic actuation by itself. The rest of the
gauge, wiring, joint, switches, plastics, adhesives, battery, SD socket and
seals still need compatibility and long-duration tests with the exact oil.
Immersion oil can migrate into connectors and sealed components; it does not
make an enclosure watertight, and water that enters can still collect at low
points. Keep the gauge's moving bucket and measurement path out of the oil and
use a sealed, strain-relieved cable entry. Do not assume a battery is approved
for immersion. Compare an oil-filled prototype with a sealed, dry, desiccated
enclosure in a humid/wet soak test, inspecting seals, water ingress, current,
event counts, and retrieval/serviceability. Check both site rules and any
environmental consequences before using oil in a cave.

[Cave Pearl's field notes](https://thecavepearlproject.org/2023/03/17/waterproofing-your-electronics-project/)
are especially relevant: their team reports using mineral-oil immersion for
some logger designs, but avoids parts with paper elements or vent openings;
they also describe water vapor permeation and water wicking along cable strands.
We do not know if these particular batteries, card sockets, adhesives, or oil
are rated for long-term immersion. In a concrete example, [Nippon Chemi-Con](https://www.chemi-con.co.jp/en/news/detail.php?id=release20241101)
reports that conventional aluminum electrolytic capacitor seals can degrade in
immersion. Do not infer from this that the nano board necessarily uses that
capacitor type: identify the actual parts first.

One candidate for no-board-modification trials is a dry sealed inner enclosure,
assembled in low humidity with fresh indicating desiccant, inside a larger
drip shield; put a strain-relieved cable through a designed sealed gland or
encapsulated wire break. Clean and dry the boards, then selectively coat only
compatible areas, keeping the SD and USB contacts clean. Compare like-for-like
units with and without the cable penetration and coating, and log internal RH
and temperature over wet/dry and temperature cycles. A dry dye or moisture
indicator can help distinguish bulk water entry from vapor exposure. Reserve
space for desiccant, battery, and a cable bend; never use an oven or wet testing
on an energized lithium battery without a purpose-designed procedure.
