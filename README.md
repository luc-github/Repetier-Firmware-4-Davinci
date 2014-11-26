##DaVinci FW based on Repetier (0.92)
============================

This fw is based on repetier fw and modified to work with DaVinci 1.0, 2.0 single fan and 2.0.
It works with host software like [repetier host](http://repetier.com), or as stand alone if you use a WIFI SD Card.

You can see more on [Voltivo forum](http://voltivo.com/forum/davinci-firmware).

Current firmware is based on version of  modified [repetier FW](https://github.com/repetier/Repetier-Firmware) 0.92 : [bgm370 Da Vinci 1.0 FW](https://github.com/bgm370/Repetier-Firmware)
Sources are [here](https://github.com/luc-github/Repetier-Firmware-0.92)
The previous version (based on repetier v0.91) can be found [here](https://github.com/luc-github/Repetier-Firmware)
It gets rid off Da Vinci software and filament restrictions: it allows to use clear ABS because it allows to disable sensors, as well as others brand name filaments because it does not use cartridge chip, it allows any slicer or third-party host software usage in normal way.

***
##Current Status
####Alpha 1 - not tested - ready for testing

***
##Installation
Use arduino IDE supporting arduino DUE, [version 1.5.8+](http://arduino.cc/en/Main/Software#toc3)
 Update arduino sources files (variants.cpp and USBcore.cpp) with the ones prensent in src\ArduinoDUE\AdditionalArduinoFiles\1.5.8
The project file is the repetier.ino located in src\ArduinoDUE\Repetier directory.
For upgrade from stock FW and revert to, please check DaVinci forum.

Do not forget to modify the configuration.h to match the targeted Da Vinci: 1.0, 2.0 SF or 2.0.
for basic installation just change :
'#define DAVINCI 1 // "1" For DAVINCI 1.0, "2" For DAVINCI 2.0 with 1 FAN, "3" For DAVINCI 2.0 with 2 FANS</code>'

  1 for DaVinci 1.0 (1Fan, 1 Extruder)
  2 for DaVinci 2.0 SF (1Fan, 2 Extruders)
  3 for DaVinci 2.0  (2Fans, 2 Extruders)

For some board having slow response time heatbed sensor that generated a defect temperature :
Set WARMUP_BED_ON_INIT to 1, it will preheat bed if it is necessary to increase sensor response time when starting printer if defect is detected, if it is real sensor issue - it will stop after 10 seconds and generate temperature defect error.

To repurpose the main Extruder cooling fan to be controlled VIA M106/M107:
Set REPURPOSE_FAN_TO_COOL_EXTRUSIONS to 1, do not for get to add a fan with power source to cool extruder permanently if you use this option.

Once flash is done <B>do not forget to use M502 then M500 </B>from repetier <B>or got select printer menu "Settings/Load Fail-Safe"</B> and accept to save in eeprom to have correct eeprom settings.

***
##TODO
[Check issue list](https://github.com/luc-github/Repetier-Firmware-0.92/issues)

***
##Implemented
* Standard GCODE commands
* Single/Dual extruders support (DaVinci 1.0/2.0)
* Single Fan / Dual fans support according printer configuration
* Repurpose of second fan usage to be controlled by M106/M107 commands on Da Vinci 2.0
* Sound and Light management, including powersaving function (light can be managed remotely by GCODE)
* Cleaning Nozzle(s) by menu and by GCODE for 1.0 and 2.0
* Load / Unload filament by menu
* Filament Sensor support for 1.0 and 2.0 (auto loading / alert if no filament when printing)
* Auto Z-probe for 1.0 and 2.0
* Manual Leveling for 1.0 and 2.0
* Dripbox cleaning
* Advanced/Easy menu (switch in "Settings/Mode" or using Up key/Right key/ Ok key in same time)
* Loading FailSafe settings
* Emergency stop (Left key/Down key/Ok key  in same time)
* Increase extruder temperature range to 270C and bed to 130C
* Add temperature control on extruder to avoid movement if too cold
* Add fast switch (1/10/100mm) for manual position/extrusion
* Preheat bed for slow response time heat bed sensors
* Several fixes from original FW
* Watchdog
* More to come ....

***
##How to test Watchdog ?
* Connect repetier host and send M281 command.
This will generate a timeout  after showing "Triggering watchdog. If activated, the printer will reset." in serial terminal.
If watchdog is enabled properly and working the printer will reset and restart.
If not, you should have "Watchdog feature was not compiled into this version!" in serial terminal and printer will not automaticaly restart.


***
##Current menu (not up to date):
Easy: <img src='https://cloud.githubusercontent.com/assets/8822552/4748170/bfa0b7e8-5a69-11e4-80b7-02b9c99fe122.png'>
Advanced :  <img src='https://cloud.githubusercontent.com/assets/8822552/4748932/bebab9e2-5a7c-11e4-8fea-cdbe3d70820c.png'>

