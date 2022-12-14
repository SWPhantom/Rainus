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
