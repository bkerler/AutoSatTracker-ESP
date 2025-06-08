# AutoSatTracker-ESP
## Autonomous Satellite Tracker with ESP8266-Huzzah

AutoSatTracker is an ESP8266 sketch that performs completely autonomous Earth satellite tracking.
All control and monioring is done over a wifi with phone, tablet or host computer via a web browser so a 
separate App is not required.

The system requires the following equipment:

    [] ESP8266: Adafruit Huzzah https://www.adafruit.com/product/2821

    [] GPS module for time and location: Adafruit https://www.adafruit.com/product/746

    [] I2C 9 DOF spatial sensor attached to the antenna to measure where it is pointed:
	Adafruit https://www.adafruit.com/product/2472

    [] PWM servo I2C controller: Adafruit https://www.adafruit.com/product/815

    [] 2D gimbal to point the antenna moved by two hobby servo motors:
	I used parts from https://www.servocity.com/

The main program is AutoSatTracker-ESP.ino which uses the following classes:

    Gimbal		control the two servos
    Circum		manage time and location including GPS
    Sensor		read the spatial sensor
    Target		compute the satellite location
    Webpage		display and update the web page

Also required are the following Arduino IDE libraries:
    
    https://github.com/mikalhart/TinyGPS
    https://github.com/adafruit/Adafruit_BNO055/archive/master.zip
    https://github.com/adafruit/Adafruit_Sensor/archive/master.zip
    https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
    https://github.com/esp8266/Arduino

    Helpful: https://learn.adafruit.com/adafruit-all-about-arduino-libraries-install-use

## Connections:

- Huzzah 3V  ..  3.3 V DC
- Huzzah EN  ..  system ground
- Huzzah 2   ..  GPS RX
- Huzzah 3   ..  GPS RX
- Huzzah 4   ..  PWM and DOF SDA
- Huzzah 5   ..  PWM and DOF SCL
- Huzzah GND ..  system ground

## Development:

The ephemeris calculations are extracted and slightly modified from P13, see below. All development
was performed on an Apple iMac using Arduino IDE 1.8.5.

During development, the web pages changed often and it was a pain to keep editing them while wrapped in
C strings in Webpage.cpp. So instead, they were moved to ast.html and ask.html as plain text then
reformatted with the perl script preppage.pl which automatically inserts them into Webpage.cpp. The script
preprun performs both insertions. This approach also allows these html fies to be viewed directly in a
browser to check html errors, proper layout and local functionality without another burn cycle of the ESP.
The web pages were tested in Chrome, Safari and Firefox on MacOS and iOS 12 and Edge on Windows 10.

### How it works:

The main loop() in AutoSetTracker.ino just polls for ethernet and GPS activity then updates the
tracking. Webpage::checkEthernet() checks for a new client connection and replies depending on
the URL they request. The default "/" returns the main web page. After rendering the page, the browser
starts an infinite loop issuing XMLHttpRequests for "/getvalues.txt" which returns many NAME=VALUE pairs.
In most of these, the NAME is the DOM id of the page element to display the VALUE. A few NAMEs,
such as T_TLE, require special handling. Page controls also issue an XMLHttpRequest POST method with
a new NAME=VALUE pair. These pairs are passed to each subsystem for action.

### Getting connected:

When first booted the ESP tries to connect to the last known WiFi station using the last IP it used.
If this fails, the ESP changes to Access Point mode as SSID SatTracker. Connecting to this and
browsing to 192.168.10.10 brings up a web page allowing entry of info needed for connecting to your
WiFi station. After Submitting this page, ESP again tries to connect to a WiFi station. This process
repeats until a station connection is successful.

Let me know what you think.

73, Elwood, WB0OEW
ecdowney@clearskyinstitute.com


### Copyright notes:
The P13 code as used here was written by Mark VandeWettering, K6HX, as part of his angst project. See
his blog postings at http://brainwagon.org/the-arduino-n-gameduino-satellite-tracker:



                   _   		The Arduino n' Gameduino
 __ _ _ _  __ _ __| |_ 			Satellite Tracker
/ _` | ' \/ _` (_-<  _|
\__,_|_||_\__, /__/\__|		Written by Mark VandeWettering, K6HX, 2011
          |___/        		brainwagon@gmail.com

Angst is an application that I wrote for the Arduino and Gameduino. It
is being distributed under the so-called "2-class BSD License", which I
think grants potential users the greatest possible freedom to integrate
this code into their own projects. I would, however, consider it a great
courtesy if you could email me and tell me about your project and how
this code was used, just for my own continued personal gratification.

Angst includes P13, a port of the Plan 13 algorithm, originally written
by James Miller, G3RUH and documented here:

	http://www.amsat.org/amsat/articles/g3ruh/111.html

Other implementations exist, even for embedded platforms, such as
the qrpTracker library of Bruce Robertson, VE9QRP and G6LVB's PIC
implementation that is part of his embedded satellite tracker. My own
code was ported from a quick and dirty Python implementation of my own,
and retains a bit of the object orientation that I imposed in that code.


## Additional tools + modifications by B. Kerler (c) 2019

- Make sure to update both sun.h and wmm.h regulary

- To update wmm.h, download WMM.COF from [https://www.ngdc.noaa.gov/geomag/WMM/soft.shtml](https://www.ncei.noaa.gov/products/world-magnetic-model/wmm-coefficients) and use tools/wmmconverter.py
  to convert to wmm.h. Next update needs to be done End of December 2020

- The sun coefficients need to be updated in approx. 2030

- If the servo moves back and forth on tracking due to measurement errors of the BNO055 sensor, make sure to mod GOOD_ERROR
  to the error degree (currently 0.8 degree) in Gimbal.h

- Before tracking, set the min and max ranges of the servo. For my servos (D645MW as Tilt and HS-785HB as Pan)
  the values 900 as min and 1900 as max were just fine.

## Components for building :

### Needed :

#### Amazon
- Satellite-Tripod with 32mm Adaption (https://www.amazon.de/gp/product/B01B5LXBLW)
- 1x Servo D645MW (https://www.amazon.de/gp/product/B01D37MH8Y) - Get a second one as these need to be modded and these can break easily :D
- 2x LM2596 DC Converter for 3.3v and 6v (https://www.amazon.de/gp/product/B077VW4BTY). Only 1 needed if you use 5v instead of 6v
- 1x PCA9685 12Bit PWM Servo-Driver (https://www.amazon.de/gp/product/B072N8G7Y9)
- 1x Adafruit Feather HUZZAH mit ESP8266 WiFi/WLAN Development Board (https://www.amazon.de/Adafruit-Feather-Development-integriertem-Akku-Ladeger%C3%A4t/dp/B01BMRTULO)

#### Servocity
- 2x 1" Bore, Face Thru-hole Clamping Hub, 1.50" Pattern 	545354
- 2x 1" Bore, Face Tapped Clamping Hub, 1.50" Pattern 		545352
- 1x 0.375" Hub Spacer 	545380
- 1x 0.250" Hub Spacer 	545376
- 5x 90° Dual Side Mount E (2 pack) 	585596
- 1x 9.00” Aluminum Channel 	585450
- 1x Actobotics® Hardware Pack A 	632146
- 1x CM-785HB-5-U Servo Gearbox 	CM-785HB-5-U
- 1x SPT-400-5-25 Tilt System 	SPT-400-5-25
- 1x 6.00” Aluminum Channel 	585446

### Optional :
- AWG 22 Cable
- DC Adapter (https://www.amazon.de/gp/product/B00VG7FH0O/ref=ppx_yo_dt_b_asin_title_o04_s01?ie=UTF8&psc=1)
- Power-Adapter 5v 4A for testing (https://www.amazon.de/gp/product/B07NSSD9RJ/ref=ppx_yo_dt_b_asin_title_o04_s01?ie=UTF8&psc=1)
- Dupont Adapter Set for Servo and Tool Connection (https://www.amazon.de/gp/product/B07QX51F3B/ref=ppx_yo_dt_b_asin_title_o05_s01?ie=UTF8&psc=1)
- Cable connectors (https://www.amazon.de/gp/product/B0758TKCRD/ref=ppx_yo_dt_b_asin_title_o06_s00?ie=UTF8&psc=1)
- 5v Battery Pack 10000mAH or 3S 50C 11.1v LiPo
- Micro-USB cable

### Pictures
- https://twitter.com/viperbjk/status/1163145242574544897
