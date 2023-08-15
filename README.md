# Rainus
Measure rain in exotic places, remote locations, away from power sources and civilization!

This frankenstein of easy-to-purchase devices lets you log rainfall data to an SD card.  
It efficiently runs off a battery and can store as much logging data as the storage you bequeath it.

This repo contains:
* Arduino code for the ESP32 C3 TTGO T-OI PLUS ESP32-C3 board
* Devlog of the project
* TODO: Posterity Images, etc

Example output data from the field-test Rainus device (headers and notes added for exaplnation):  
[Google Sheets](https://docs.google.com/spreadsheets/d/1Qej0Jb0RguejD68-Whe8UmjqHHGkIUU_zKdhaDspYHQ)

Currently, this is designed to work with a [rain gauge like this](https://www.scientificsales.com/6466-Davis-AeroCone-Rain-Gauge-with-Mountable-Base-p/6466.htm)
Essentially, it's a device that closes a circuit. The ones we currently have use an RJ12 6p6c connector, and the Rainus is (unfortunately) built around it. 

Generally speaking, though, the Rainus Does A Thing when a circuit is closed.

## Setup
1. Follow the setup steps on the (LilyGO Github page for the T-OI-PLUS board)[https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS]
1. Connect a Rainus to the computer with a USB-C cable (make sure it can transfer data... many don't...)
1. If using ArduinoIDE, open the Serial Monitor and set the BAUD rate to 9600
1. At the top, select a `wusb...` entry in the connected devices and set the device to `TTGO T-OI PLUS RISC-V ESP32-C3`
1. Upload the Rainus.ino file and look at the Serial Monitor. You should see Rainus chatter!

### Steps of Operation
* An external device connects VCC to a pin.
* Rainus wakes up from a deep sleep
* Checks if the SD card is present and accessible
* Checks if the RTC is present and accessible
* Read from sensors, if attached/available
* Writes a log to SD card with the current time, the chip id, etc


## Rainus 1.0
Parts soldered onto a hand-etched PCB. Single board, stuffed into a cheap, plastic food container.
8 units deployed to Thailand!

A 16340 battery and SD card are replaced on a schedule, where our Thai cave-master, Nong, treks to the Rainuses, opens up the sandwich box, and replaces the battery and SD card in-situ.

There are problems with Rainus 1.0:
* Excess moisture in the caves seems to be shorting traces, draining the main 16340 battery, and maybe draining the RTC coincell battery.
* Replacing the SD card and battery in a cave environment is fiddly work.

## Rainus 2.1 (Deprecated)
Several upgrades over 1.0:
* Increased battery: 18650 batteries are larger, seemingly more ubiquitous, and less likely to be sold under false pretenses...
* Waterproofing via [Silicone Conformal Coating](https://www.amazon.com/gp/product/B085G42TGS)
* PCB designed in KiCad, and manufactured through jlcpcb

This version of the Rainus is split into two sections:
The Mother unit and the Child unit.

The Mother has the ESP32 board and the RTC, which keeps the time. It also has female RJ12 and RJ45 ports to connect to the rain gauge and the Child, respectively. The child is enclosed in a non-waterproof box, with incoming cables going through a hot-glued (or equivalent) slit in the box. 

The Child has the SD card reader with SD card and the battery. It has a single RJ45 port that connects to the Mother. The Child is enclosed in a waterproof box, with an ethernet cable that goes through a gasket-sealed port.

The ethernet cable coming out of the Child goes into a waterproof Ethernet coupler. Another ethernet cable connects to the other side of said coupler and connects to the Mother. 

The intent is for our Cave Technician to come to the Rainus device with a replacement Child box (with a recharged battery and empty SD Card), disconnect the waterproof ethernet coupler between the current Mother and Child, and replace the old Child with the new Child, and reconnect the boxes. The old child will then be taken to a controlled location (without cave moisture...), and the battery and SD card will be taken out and processed.

A Grove moisture/temperature sensor may be added to the Mother unit. Waiting for deliveries from Seeed now.

There are problems with the Rainus 2.1:
* Separate units, connections between units, etc are more costly.
* Multiple units are more bulky to ship.
* If we change the RJ12 plug coming out of the rain gauge to two plugs that can be plugged into waterproof ports on a box, we don't have to worry about moisture and greatly simplify the Rainus design.
* COST.

Current transient problem:
* When the Child unit is disconnected (or the battery removed), the RTC doesn't swap to its coincell battery quickly enough. I saw on some forums that this can be solved by adding an `rc network` to the VCC input, to allow the RTC to switch between the VCC to battery. I'll need to redisign the PCB to have that.
* Though most parts of the boards are covered in silicone, the USB-C port is not. This may be problematic still. May need to stuff the port with a rubber thing. We shall see!

## Rainus 2.2
Back to a single-board design!
Board designed to fit into a [plastic enclosure with a gasket](https://www.amazon.com/dp/B07RTYYHK7).
Two copper wires stick through the enclosure and act as terminals to connect the rain gauge wires to (soldered to alligator clips).
A grove connector and temperature/humidity sensor also sticks through the enclosure to read the environment around the unit.


## Bill of Materials
* LILYGO® TTGO T-OI PLUS RISC-V ESP32-C3
[Aliexpress](https://www.aliexpress.us/item/3256803168396369.html)  
[Github](https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS)

* PCF8523 RTC
We ended up using a Raspberry Pi-style header PCF8523 RTC. This was ONLY due to shipping availability and price at the time.  
Using a more sane pin style would have made some of the PCB design nicer
[Amazon](https://www.amazon.com/dp/B072DWKDW9)

[What we Should(??) have used](https://a.co/d/cd6rMIO)

* MicroSD Reader/Writer
Bought a couple different ones. These worked right away and came with pin headers, which were great. Small footprint, also good.
[Amazon](https://www.amazon.com/dp/B08CMLG4D6)

There are SD card readers with a nice spring detent, that are bigger.  
Those, however, didn't work and would not be recognized as working SPI devices.

* MicroSD Cards
32GB microSD cards. Bought the cheapest ones that had an appreciable capacity.
[Amazon](https://www.amazon.com/dp/B07RVFZ3F3)

* Perfboards/PCBs/etc
JLCPCB-manufactured PCBs to mount our breakout boards on.

This is a collaboration with [George](https://github.com/kxygk), in order to help with his Monsoon PhD research in Thailand.