2022-09-16 15 48
GK and I both have our Arduino IDEs writing to the TTGO DOI Plus

Trying to get the SD card writer/reader to work. Using PulseView to debug which pins are used for the SPI protocol.

References:
SD Card
https://randomnerdtutorials.com/esp32-microsd-card-arduino/
https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS/blob/main/schematic/T-OI_PLUS_Schematic.pdf
https://itp.nyu.edu/physcomp/labs/labs-serial-communication/lab-data-logging-with-an-sd-card-reader-using-spi-communication/
https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/spi_master.html

ESP32C3/TTGO T-IO
https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS
https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS/blob/main/schematic/T-OI_PLUS_Schematic.pdf
https://www.studiopieters.nl/esp32-c3-pinout/

I2C/RTC
https://create.arduino.cc/projecthub/electropeak/interfacing-ds1307-rtc-module-with-arduino-make-a-reminder-08cb61

Deep Sleep
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html



GK:
Getting the board to be flashed was problematic.
Did NOT install drivers.

Added gk to the dialout group in Linux. 
Added read/write perms to ttyusb0.
  Noticed that the first time the device was plugged in, it routed to ttyusb0. BUT second time it was connected, it was routed to ttyacm0 device.
Added read/write perms to ttyacm0 as well.

Confusing part was the extra remapping to acm0


2022-09-17 00 42
Much blood was spilt

SD card is read and written to.
I did REALLY well. I DESERVE a shake. Denny's??
It IS open right now.

Lessons learned:
File names need leading slashes. Files need to be closed to be saved.
GK adapted some example code into other code.
vspd->fspd in one example, then use the spi init code in an SD example code.

We had issues with pins.
2, 4, 5, 6 for MISO, MOSI, CLK, SS respectively seemed to work.
Plugged the board into breadboard, to break out into the SD reader and the logic analyzer was a good move. Analyzer claws are neat, but finicky.

We took a walk. It helped (maybe).


Checkpoint: Can write files to SD card now.
Next, need to get the other SD card reader working (MAYBE)
REALLY next, need to work on the RTC integration.

Then, button press to get out of sleep, do the reading and writing, and done.


2022-09-17 11 21
Waking up.

Light day today- looking at RTC usage, then temp/humidity sensor, to finish implementation tomorrow.

Woe is us!
Writing to the file in the loop results in an empty file.

Reverting to only writing to the file in the setup function works.

Found the issue- we were opening the file even after we were done. No corruption, but the file was blanked.

Solution: FILE_APPEND
    txtFile = SD.open(filename, FILE_APPEND);

vs
    txtFile = SD.open(filename, FILE_WRITE);

as per
https://forum.arduino.cc/t/appending-datas-to-file-instead-of-recreating-the-file-w-sd-h-library/467651/9


2022-09-18 13 07
Got the I2C RTC working using 
https://create.arduino.cc/projecthub/electropeak/interfacing-ds1307-rtc-module-with-arduino-make-a-reminder-08cb61
Had to initialize the RTC and start its running with
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));


2022-09-19 00 13
Checkpoint message. Refactored code to be neat.
We now log the times cleanly, along with the ID of the chip.

2022-09-19 00 44
Starting to add button functionality.

2022-09-19 01 09
Finished button functionality.
Next up, deep sleep!

Temp/Humidity later.
Ordered other RTCs, also



2022-09-19 14 28
Starting deep sleep work
Thinking about too many clicks too fast, of using an exponential backoff, etc

16 18
Done with deep sleep
Added todos. Added debug printing switch.
Did a few 4 17 button presess
1x 4 18
2x 4 21
1x 4 22
3x 4 27 (see 6)
4x 5 37 (see 8, with 2 clicks at 5 36)
6x 5 58 (see 11)
reset at 5 58
3x 5 58 (SEE 4 (could be... 3)

Will come back later to do more


TODO:
* Hook up battery to terminals, see if we get ok 5v power. If so, no need to stress over RTCs
* String concatenation instead of 12 prints to file.
* Cases for different part removal in the field (battery, SD card, etc)
* remap pins for optimal placement of SD card, RTC
* Ideate on case/fixturing
** temperature/humidity


2022-09-19 18 03
Tested the program over time. Need to add logic where the logger:
Wakes up on rain gauge button press, logs data, waits until the button is NOT pressed again, THEN goes to sleep.

Another test, after debounce logic:
3x 6 16
5x 6 17
hold @ 6 33
12x 6 34

Worked!
Holding worked well, all data looks a-ok:
```
7598744,2022-09-19T18:16:45,1663611405,716926605
7598744,2022-09-19T18:16:46,1663611406,716926606
7598744,2022-09-19T18:16:46,1663611406,716926606
7598744,2022-09-19T18:17:06,1663611426,716926626
7598744,2022-09-19T18:17:06,1663611426,716926626
7598744,2022-09-19T18:17:06,1663611426,716926626
7598744,2022-09-19T18:17:06,1663611426,716926626
7598744,2022-09-19T18:17:07,1663611427,716926627
7598744,2022-09-19T18:33:48,1663612428,716927628
7598744,2022-09-19T18:34:19,1663612459,716927659
7598744,2022-09-19T18:34:20,1663612460,716927660
7598744,2022-09-19T18:34:20,1663612460,716927660
7598744,2022-09-19T18:34:20,1663612460,716927660
7598744,2022-09-19T18:34:21,1663612461,716927661
7598744,2022-09-19T18:34:21,1663612461,716927661
7598744,2022-09-19T18:34:21,1663612461,716927661
7598744,2022-09-19T18:34:22,1663612462,716927662
7598744,2022-09-19T18:34:22,1663612462,716927662
7598744,2022-09-19T18:34:22,1663612462,716927662
7598744,2022-09-19T18:34:23,1663612463,716927663
7598744,2022-09-19T18:34:23,1663612463,716927663
```

18 39
String concat?

2022-09-20 0101
No string concat.

GK and I built different versions of the assembly. We reassigned pins to connect the SD card reader directly. 
Looks very good and shoddy at the same time.

We have a cluster of wires soldered to a header pin. Not pretty. Could use the battery negative terminal or something.

TODO:
We may need to disable the wifi and bluetooth.


01 10
Let's measure voltage of battery. 

01 14:
Voltage: 4.13-4.14v

2022-09-20 10 51:
Voltage: 4.12-4.13v

Did 10 readings.

2022-09-20 17 01:
Did a score of more readings
Voltage: 4.11-4.12v




2022-09-27
Tested and added code to make the Arduino RTC work
  RTC_PCF8523 (https://learn.adafruit.com/adafruit-pcf8523-real-time-clock/rtc-with-arduino)

  
2022-11-16 14:40:00
Just discovered that the local test Rainus is no longer glowing with the on LED.
As I assumed when I put in a non-spec 3v Energizer Lithium 12-2032, when voltage dipped below some threshold, the board shut down.
This must have hapened between November 9th (the last time I pulled data from the card, when the light was still on), and today.
Since I put the device outdoors on approximately October 19th (judging by the first data in [the spreadsheet](https://docs.google.com/spreadsheets/d/1Qej0Jb0RguejD68-Whe8UmjqHHGkIUU_zKdhaDspYHQ/), this shows that with a 3v non-spec battery, the device will work at LEAST 21 days straight, up to 28 days, assuming the battery went below the threshold today...

Let it be known that I put an [in-spec 16340 battery](https://www.amazon.com/16340-Battery-Batteries-2800-Pack/dp/B0BK4KY88J/) into the Rainus device at my house. at the above timestamp


2023-01-01
Some retroactive notes:
The in-spec battery did NOT hold up. Not sure if it was because I didn't charge it or if it was a trash battery at the time.
In fact, I went through two of these batteries and each lasted 1 week or less. I didn't check often, so not sure when the power cut out...

Before going to Thailand to check in on the other Rainus devices and meet with GK on 2022-12-08, I put in the other alkaline Duracell battery (3v rated). THAT one lasted a longer time than the other 16340 batteries. That said, by 2023-01-01, I noticed the battery was dead. 

Another BIG note:
Looking at the previous commit, you'll see that the latest batch of Rainus updates resets time!
This indicates that the line
`if (!rtc.initialized() || rtc.lostPower()) {`
was true and the RTC's time was adjusted to the compile time... which suggests that the RTC battery went below operational theshold...
This doesn't really make sense, and needs to be tested... like immediately. 
GK and I saw similar results in Thailand, though we assumed that the battery drained due moisture shorting.

2023-01-01 17:20:00
Now that I look over the data again, I see that not only did the RTC reset twice in the new dataset, but ALSO that it seems to have reset after I was already gone!
This doesn't seem to make sense. As far as I thought, if the main battery is plugged in, the RTC doesn't reset and ONLY resets if both batteries are out at the same time.

This indicates that my test device needs to go in and a documented, rigorous test should be done:

1. Take out RTC Coin Cell Battery (rtcbat) and 16340 Main Battery (mainbat) 
  - This should set the RTC to unpowered/not started.
2. Turn off the board and put in charged rtcbat and mainbat
3. Connect board to a computer with the latest rainus.ino in Arduino and flash it.
4. Press the rain button a few times over a few minutes.
  - Readings should increment in time as expected.
5. Remove mainbat. Wait a few minutes.
6. Put in mainbat.
7. Press the rain button a few times over a few minutes.
  - Readings should STILL increment in time as expected. Time should NOT have reset.
8. Remove rtcbat. Wait a few minutes.
9. Press the rain button a few times over a few minutes.
  - [ ] Verify what happens here. I expect that the time would have reset on every button press???
10. Put in rtcbat. Wait a few minutes.
11. Press the rain button a few times over a few minutes.
  - [ ] Readings should have reset again, BUT will now increase in time as expected.
12. Remove rtcbat and mainbat. 
13. Put in rtcbat and mainbat.
14. Press the rain button a few times over a few minutes.
  - [ ] Readings should have reset again, and will increase in time as expected.

Further, I'll need to comment out the battery time reset logic. If the RTC's `rtc.lostPower()` is true, it may still carry a memory of the last time???
I'm pretty sure if `rtc.initialized()` is true, the time has been wiped, though.

I should read the specs!

Either way, if the RTC had an issue in time, we should write that to the SD card. 
eg, if lostPower() is true,
```
1642488,2022-10-19T17:29:31,1666200571,719515771
1642488,2022-10-19T17:29:45,1666200585,719515785
LOST POWER. CURRENT TIME: 2022-10-19T17:29:51
```
and keep logging normally. Will need to check if `rtc.start();` needs to be called after that, though.


2023-04-27
Rolled-up update!

We got the blessing to do Rainus2!
Created the loggo project/service that can get arbitrary logs. Making the Rainus devices send HTTP POSTs when testing.

Will be making new versions of the Rainus devices. Alternate chips (non-pi RTC, etc), alternate batteries, added dc-dc converter???



2023-05-06
Soldered all the liligo boards to pins.
Noticed that they are all version 1.3!

Replaced the pi header PCF8523 with the HiLetgo one:
https://www.amazon.com/gp/product/B082LTHR28

Learning to use the Power Profiler Kit II:
https://www.nordicsemi.com/Products/Development-hardware/Power-Profiler-Kit-2


Hooked up Rainus to the PPK2 and found that idle Amp usage:
1-2mA
max spikes to 100mA

With a 2500mAh battery, it would last between 50 and 100 days.


2023-05-11
Ordered first version of boards from JLCPCB:
OrderId: Y1-6005729A

2023-05-17
Boards arrived! Extremely fast shipping. 
Put together one of the prototype boards.
Takeaways/discoveries/todos:
* Make sure the through hole diameters are larger. .6/.7 work, but they're a bit tight for the pins I have.
  * .9mm should be great.
* RJ12/13 connectors do NOT fit into JR45.
  * Order and use JR13 connectors Get the 6 pin 6 position ones.
* I routed the data cables backwards for the SD card reader...
  * Changed the Rainus compile code.
* No ground plane.
  * Rerouted connections and added groundplane.
* Battery power on Mother board awkwardly placed.
  * Routed power directly under the Lilygo's power inputs for short, easy-to-solder connections.
* Corners rounded for next version.
* Improved silkscreen layer
* Replaced/rerouted resistor. Using an 0805/2012 4.7K SMD Resistor
* Separated connections to different layers, to minimize roundabout paths.

2023-05-21
Put together one of the 10 mother/child boards!
Using ethernet cables to connect them.
Manually soldering conductors to through holes. Tedious process. I should use actual Ethernet jacks...

Just deployed the board outside with an 18650 batter that's measuring 4.14V

2023-06-10
Replacing the test rainus battery
Old Battery: 3.97V (.17V drop)
New Battery: 3.92V

2023-06-07
Rainus 1.2 boards ordered.

2023-06-17
Rainus 1.2 boards left LA import facilities! Should be here this week.
Need to try out different containers after a sample board is assembled.

Maybe different containers for the mother and child boards.
Child board is more water-sensitive, due to SD card and battery. I bought
`MAKERELE Wire Strain Relief NPT 3/8”Nylon Cord Grips Cable Glands Cable Gland Joints`
from Amazon, which should keep water ingress minimal with the child board.
I also ordered
`ANMBEST 10PCS M23 Shielded RJ45 Waterproof Cat5/5e/6 8P8C Connector IP67 Ethernet LAN Cable Connector Double Head Outdoor LAN Coupler Adapter Female to Female`
Which are laaarge, but should make the interface between the ethernet cables free from water ingress, too.

Idea will be to have a completely water-resistance child board module, which will consist of



                  __________________
LAN coupler      |child board in box|
==}-------------[|]-¬ ___           |
          strain |  L|   |[===]     |
          relief |   |___|    batt  |
                 |   []sd           |
                 |     card         |
                 |__________________|


I'll make 2x of these than the Rainus main boards, so that these can be swapped out and have their SD cards replaced in a non-wet-cave environment.

I'll have to build the ethernet cables AFTER they go through the strain relief gland, as the male ethernet plug will not fit through...


2023-06-22
Put together a mother board and 2 child boards to test out swapping out the child boards.
Issue found!

Looks like when we disconnect power from the board quickly, the RTC doesn't have enough time to swap power from vcc to battery:
https://www.nxp.com/docs/en/data-sheet/PCF8523.pdf (page 53-54). We should include an "rc network on the vcc pin"... So I need to figure out what that means and how to do it >_>

Furthermore, geokon and I talked about the possibility of simplifying this project by changing the RJ13 cable ffrom the rain gauge to two plugs.
That way, the whole Rainus system can be on a single board again, inside of a waterproof container, and have two connectors resistant to water ingress
to plug the rain gauge wires into.

Need to test how I can do that and how the process can be repeatable.


2023-06-25
[DHT 20 Seeed Temperature/Humidity Grove sensor](https://www.seeedstudio.com/Grove-Temperature-Humidity-Sensor-V2-0-DHT20-p-4967.html) working! The Lilygo board doesn't use the correct layout for its Grove connector, so I had to shift around the crimped wires.

Things to do/think about:
1. Power usage with the new sensor
2. If using sensor, we DO want to expose it to the surroundings. How to do it? Either have a slit in the box with the less-sensitive parts (if keeping the 2-box system)
3. If using sensor, and if sensor is borked, gonna log default -666.0 values for temp and humidity, but otherwise log, cuz it's a secondary measuring thing
4. If using sensor, and using two boxes, probably keep using the rain gauge RJ12 connector
5. If not using sensor, how to connect two separate wires from the gauge to the box in a waterproof way?
6. If using sensor, AND we want to use a single, waterproof box, how to connect SIX separate wires (4 for Grove, 2 for rain gauge) in a waterproof way to box?

2023-07-01
Designed and ordered the v2.2 Rainus boards!

This is what geokon and I decided:
This version of Rainus:
* Contained in one box.
* Made of the following sensors/parts:
ESP32 dev board
SD Card Reader/Writer + SD Card
PCF8523 RTC + CR1220 battery   <-- Modified to have a solder bump on the board's negative battery terminal
Seeedstudio Grove - Temperature & Humidity Sensor V2.0 (DHT20)
18650 Battery Holder + 18650 Battery
4.7k resistor
Rain Gauge
Rain-proof project box (~100X68X50mm)
Wire
* Will have the Temperature/Humidity sensor on the outside of the box, with its wires going through a drilled hole in the box.
* Will have two copper terminals extruding from the box through drilled holes. These are used for the Rain Gauge wires to connect to.
* The holes will be filles with water-proof epoxy/silicone/other sealant.
* The Rain Gauge wire will be stripped and split into two parts, with each soldered to an alligator clip, or similar.
* The battery holder, battery, and SD card will be easily accessible when the box is serviced.

The Rainus box will be wholly replaced when Nong does his rounds. The Rain Gauge's clips will be unclipped from the exposed terminals, the box will be replaced, and the clips will go on the new box's terminals.

The box will be taken to a safe, dry location and will have its 4 screws removed. Then, the 18650 battery and the SD card can be replaced.

2023-07-09
A couple weeks ago, I found Paul Price on youtube: https://www.youtube.com/@paulprice . He has long design and assembly videos for his electronics projects.
Great channel and I've been watching a bunch of his streams to prepare for Rainus 3 work.
On 2023-07-05, he came out with a video: https://www.youtube.com/watch?v=vbtSqaVAqRw
He built a data logger that looks like it matches all our needs!
I talked to him, and got a couple boards to do tests on.
If the Rainus 2.2 boards do not work for this phase of research, the Bee Data Loggers will be a fantastic fallback.
In the future, they'll be great boards to scale up the project.

I will still be designing the Rainus 3 board, but trying to find a niche that doesn't overlap with this board (waste of effort and it'll likely be too expensive, time-wise to just redo that board).
However, a board that matches or improves on the (miniscule) power draw of the Bee Logger, with a simpler power bus, using the small non-antenna ESP32 C3 chips will be a good design and assembly endeavor.

https://github.com/strid3r21, for reference!

2023-07-10
Received Rainus 2.2 boards!

2023-07-11
Desoldered all (-3) Rainus 2.1 boards to get the ESP32 dev boards free. Terrible experience. Needed to use a hot air gun and pry the dev boards off. 

2023-07-12
Had an issue where EVERY ES32 v1.3 board (even the new ones) was not flashing with the following message.
`A fatal error occurred: Unable to verify flash chip connection (No serial data received.)`
Which was weird, as I was actively getting Serial data in the Serial monitor with the boards running.
Searched around, saw that most people said it was a hardware issue. I thought that my desoldering method cooked the ESP32 chips, or the Flash chips.
However, because the new boards were also not flashing, this was unlikely (unless the new boards were shipped bad...?)

Anyway, I later realized that, when connecting the dev boards to my computer and Arduino IDE and selecting a Board and Port, there are 2+ choices for connecting.
Specifically, the USB ports:
* /dev/cu.usbserial-56581020151 Serial Port
* /dev/cu.wchusbserial56581020151 Serial Port // USE THIS ONE FROM NOW ON

In short, the wch... one was the one that DIDN'T give me upload errors.
I could have sworn that for the esp32 v1.2 boards either one worked, but my memory is hazy...


After the relief that the dev boards are a-ok, I put together one Rainus 2.2 board (without waterproofing/drilling the enclosures)

Spent the day touching the gauge terminals together and logging temperature and humidity around my path in Seattle. Looking good so far.

2023-07-13
Water stress-test:
* Put waterproofing silicone on the Rainus 2.2 components and breakout boards
* Drilled holes into the enclosure (two small holes for the rain gauge terminals, and a larger hole for the Grove wires)
* Put J-B Weld Plastic Bonder Epoxy in the egress holes
* Sealed the box and submerged it until water came over the seal and sealed Grove hole
* Attached the Rain Gauge (with wires stripped and soldered to copper alligator clips) to the exposed terminals on the outside of the box
* Set my sink to drip water into the Rain Gauge.

2023-07-14
The Rain Gauge activated every 12-14 seconds. 
Started at
0045
Ended at
1122

Approximately 38,220 seconds

That SHOULD be 2940 clicks

The rainLogOvernight.txt shows 2613 logs during that time!
Gonna do some data analysis!



Thoughts/improvements:
* When the Grove sensor is not connected, the Rainus doesn't work, and breaks when trying to initialize it. 
  I'll need to make my own version of the DHT.cpp file. If not initialized, gotta return default values, but let logging keep happening.
* geokon doesn't like the wires attached to the lid. I agree that it's not the best, but I'm thinking about ease of disassembly. Gonna try to run the wires UNDER the board and come out under the box.
* Gotta give the SD reader a little more room with its pins. Current angle makes it less-than-perfect during SD/battery swap.

2023-07-16
Wrote python script to parse rainus output.
Analyzed the drip data and found that the drip rate slowed down throughout the night. The result data looks good!

![overnight test graph](images/overnightDripTestBreakdown.png?raw=true "Overnight Drip Test Results")


2023-08-14
1st Rainus v2.2 adventures:
Had the same battery in it since 2023-07-12
Currently at 4.07v
Unfortunately... I'm not sure what the battery voltage was exactly when I started, but the other batteries, which had not been plugged in are all between
3.96 to 4.14 (most between 4.01 and 4.08)
That means that the battery, at worst, went down .7 more than the most charged battery.

That SEEMS like good news!
Not sure what the battery cutoff will be for the ESP32 board, but it will LIKELY last for more than 3 months, assuming a well-built battery.

Over that time, we logged about 38000 entries (2.4MB)!

Some days of operation, the Rainus withstood 16 to 47 degrees celcius and rain directly on the outside of the box and temp/humidity sensor!

Still ticking.

One thing to think about is the blackness of the box. If in direct sunlight, it may give weird temperature readings, as the temperature sensor is facing
the top of the box, which probably heats up extra when in direct sunlight. Maybe Rainus boxes meant for outdoor use should have a foil tape taped
under the rain sensor, so as not to heat up from direct sunlight.


Also, waiting for 4 rain gauges to arrive at my house.

My currect Rainus v2.2 status:
1 Complete
2 In Boxes, needing to get programmed
17 Out of boxes, need to have their boxes drilled and sealed, need to get programmed, and may need to have their terminal wires put on backwards,
so that the lid of the box has the sensor and terminals.

Learnings/TODOs
* Putting the terminals and sensor on the bottom side of the box, with wires routed under the circuit board looks nicer, but is a horrible pain to assemble. Pity to the ones that have to take the boards out to program them later...
* Need to order 12 alligator clips to send away to Thailand.
* I ran out of SD card readers! Surprising! But have the perfect amount for the 20 to be sent
* Need to order more SD cards, likely...
* The Grove wires need to be swapped very specifically ESP32-side. Otherwise, the ESP32 board and/or the temperature/humidity sensor gets PROBMEMATICALLY hot...


2023-08-20
Got news a couple days ago that the extra rain gauges are not coming to my house YET. Will see if that resolves soon.

Found a different DHT20 library to use for the temp/humidity sensors: https://github.com/RobTillaart/DHT20
Changed the code to log values if found and log default values if not found.

Found an issue with the RTC time setting... Unfortunately, the compile time is assumed to be UTC, but uses local time. I added a UTC offset in the code so that whoever compiles the sketch will set the RTCs to UTC time. This SHOULDN'T be necessary, now that the units have been set, but needs to be called out.

Cleaned up code. No more web/timer stuff. Hopefully it's a LITTLE cleaner...

2023-08-29
Another check of the battery we've been using in the test unit:
3.89v


2026-11-09 and 2026-11-10 — CH32V203 prototype and power investigation

The 20 ESP32-based loggers deployed in Thailand mostly failed within a year;
moisture and corrosion were common, and some units had unexplained failures or
unreliable readings. We started a new prototype on the nanoCH32V203 v1.1,
which has an RTC and microSD socket. The goal is a low-cost, maintainable
logger that records rain-gauge reed-switch closures for up to two years.

The board can now be built and flashed on macOS and Linux. A PA0-to-GND
closure produces an event record; the user verified records on the card. The
CSV columns are `chip_id,event_unix_utc,build_unix_utc`. The ID comes from
the chip's factory identifier, not the computer doing the flashing. The RTC
is initialized from the build time if needed. After a complete loss of power,
it may restart at the old build time, so timestamps can be wrong until reset
with a fresh build; the build time is a reference, not a trusted current time.

The current field firmware uses STOP and wakes on a PA0 falling edge. An
interrupt captures events even while an SD write is in progress, with a finite
queue. Ten events are buffered in RAM before writing the card. Power loss can
discard a partial batch, and an interrupted SD write can damage more data.
The blue LED can be disabled at compile time; with it enabled, repeating
groups of 2, 3, 4, or 5 flashes indicate RTC, SD mount, queue overflow, or SD
write errors respectively. The debug USB build stays awake and is not the
field power measurement.

On cold power-up, the RTC sometimes failed to synchronize (the temporary
diagnostic reported eight flashes), although pressing reset fixed it. Longer
RTC synchronization waits did not fix it. Selecting the LSE source and
enabling the RTC in separate register writes fixed the observed cold starts;
several cold starts and the normal firmware were checked successfully. This
is consistent with the documented ordering requirement, though a few bench
trials do not establish long-term reliability.

With a Nordic PPK2 supplying 5 V to the board and a Gigastone 32 GB microSD
card, we measured the following settled currents. These are approximate
whole-board measurements, not individual chip or card specifications:

| Test firmware and condition | Current at 5 V |
| --- | ---: |
| Minimal standby, no SD card, RTC off | 32 µA |
| Same standby, card inserted but not initialized | ~1 mA |
| Card initialized, standby, RTC off | 1.14 mA |
| Card initialized, STOP, RTC off | 1.10 mA |
| Same STOP test with logger-like GPIO configuration | 1.09 mA |
| Earlier normal logger sleep sequence, card present | 1.95 mA |
| WFI experiment, card present; did not wake on PA0 | 1.07 mA |
| Revised WFE experiment, card present; PA0 wake worked | 1.05 mA |
| Normal logger with revised WFE, card present; PA0 wake worked | ~1.1 mA |

The WFE revision clears a pending event before sleeping and checks for an
edge while entering sleep. It reduced measured idle current without losing
the observed wake behavior; the exact cause of the earlier higher current
is not proven. A roughly 28 mA peak was seen during activity, plausibly an SD
write, but its duration and attribution were not measured. The card/no-card
comparison strongly motivates testing cards and storage alternatives, but
does not isolate card current from pin and initialization effects.

Archived test sources and reconstruction instructions are in
`ch32v203/experiments/2026-11-rtc-and-power/`. The stripped-down standby
program is also kept in `ch32v203/power_floor/`. Several diagnostics cannot
log events and deliberately stop the RTC; they are not field firmware. See
`ch32v203/POWER_AND_ENCLOSURE.md` for the power and enclosure notes.

Still to do: compare SD cards under the same conditions and measure energy per
write; test exact event counts with the actual wired gauge, including events
during writes and around sleep entry; test abrupt power loss and SD recovery;
address clock setting after power loss; evaluate durable low-power storage,
the complete battery/power path, and moisture protection. Two-year operation
remains a target, not an established battery-life result.

2026-11-12 — SD card measurements and startup investigation

The Gigastone card's observed write interval was approximately 200 ms with
about 2.5 mC of total measured charge. A separate, unbranded 4 GB card gave
approximately 190 µA whole-board idle current. The role of card capacity,
controller, firmware, and board interaction is not established.

A 64 GB SanDisk Extreme caused the normal logger to repeat the three-flash SD
mount error, despite the Mac reporting an MBR partition map, 512-byte device
blocks, and a partition labeled FAT32. Read-only diagnostic firmware located
the first failure before CMD0: the driver waited for an SPI-ready indication
before sending the command that switches the card into SPI mode. Bypassing that
check made it mount after RESET but initially still fail after a cold start.
A 100 ms delay alone did not fix the original pre-command ready check; adding
that delay to the bypass still failed cold. Retrying CMD0 after the bypass and
delay mounted the card in a cold-start test and after RESET. The exact reason
the first CMD0 response was unexpected is not known.

The normal driver now includes that startup delay, skips the pre-CMD0 ready
check, and retries CMD0 for a bounded period. Existing host tests pass. With
the normal logger flashed, the user reported no startup error flashes and
confirmed that event logs appeared on the SanDisk card. Repeated cold starts,
previously working cards, and longer logging runs remain to be verified before
deployment. The read-only diagnostics and their blink codes are archived in
`ch32v203/experiments/2026-11-sd-mount/`.

2026-11-13 — SD card comparison

The user compared five microSD cards with Rainus. Each was formatted on the
Mac using `diskutil eraseDisk FAT32 RAINUS MBRFormat /dev/disk4` for that
session. The disk identifier must be rechecked for each card: `eraseDisk`
erases the selected whole disk. The supply voltage, firmware/LED setting,
and precise PPK2 selection boundaries were not stated with these results;
the currents should be treated as whole-board observations, not isolated
card measurements.

| Card | Idle | “Log” current | Write peak | Write current over interval | Estimated total charge in interval |
| --- | ---: | ---: | ---: | --- | ---: |
| Lerdisk 2 GB | 222 µA | 1.5 mA | 30 mA | ~8 mA over 600 ms | ~4.8 mC |
| Lerdisk 4 GB | 225 µA | 1.5 mA | 30 mA | ~8 mA over 550 ms | ~4.4 mC |
| Lufasn-D 4 GB | 735 µA | 1.4 mA | 22 mA | 8.8 mA average over 472 ms | ~4.15 mC |
| Cloudisk 4 GB | 233 µA | 1.7 mA | 25 mA | 7 mA average over 550 ms | ~3.85 mC |
| Bliksem Pro Plus 8 GB | 707 µA | 1.84 mA | 43 mA | 22.8 mA average over 325 ms | ~7.41 mC |

The charge figures are calculated as current × time, assuming the reported
current is the average over the entire selected interval. They include idle
current during that interval, and are not the incremental write-only charge.
If the short peak was outside the averaged interval, its charge is excluded.
The user observed that the log-entry waveform has a peak, falls, then rises
again in a smaller, slower peak before declining. Its cause has not been
isolated. Measurements of multiple units, cold starts, successful logs, and
longer runs are still needed to assess repeatability and reliability.

The user later measured approximately **625 µC for an event without an SD
write**, with the event LED on. This is whole-board charge over the selected
activity interval; its duration and how much of the charge is above the idle
baseline were not specified. It is a measurement of the earlier firmware,
which lit the LED for 100 ms on every event. The firmware was changed so the
LED is on during batch SD writes instead, while retaining error flashes.
The 625 µC value must not be assumed to apply to the new firmware without a
new measurement.

The SD-write LED was subsequently removed as well. The blue LED now stays off
during normal event capture and batch writing, but still flashes error codes
when `RAINUS_LED_ENABLED` is set to 1.

With normal-operation lights off, the user subsequently measured a Lerdisk
card at a **1.14 mA peak and 22.6 µC total charge for a log event**, and a
**22.8 mA peak and 3.24 mC total charge for an SD write**. The card capacity,
supply voltage, measurement durations, and whether the write selection
includes the triggering event were not specified for this measurement. These
are whole-board charge readings over the selected intervals, including idle
current; they should not be added to a continuous idle estimate without
accounting for that overlap.

The user also tested appending to a file with approximately **6,300,000
entries**. The observed operation took approximately **15 seconds** and
used **113 mC** of measured charge. The card, supply voltage, selection
boundaries and exact contents or layout of the test file were not specified
with that result. For comparison, the current logger opens `RAINLOG.TXT`
in append mode and closes it for each batch; the cause and repeatability of
the long append still need investigation.
