# Rainus
Measure rain in exotic places, remote locations, away from power sources and civilization!

This frankenstein of easy-to-purchase devices lets you log rainfall data to an SD card.  
It efficiently runs off battery and can store as much logging data as the storage you bequeath it.

This repo contains:
* Arduino code for the ESP32 C3 TTGO T-OI PLUS ESP32-C3 board
* Devlog of the project
* TODO: Posterity Images, etc

Example output data from the field-test Rainus device (headers and notes added for exaplnation):  
[Google Sheets](https://docs.google.com/spreadsheets/d/1Qej0Jb0RguejD68-Whe8UmjqHHGkIUU_zKdhaDspYHQ)

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
TODO

This is a collaboration with [George](https://github.com/kxygk), in order to help with his Typhoon PhD research in Thailand.
