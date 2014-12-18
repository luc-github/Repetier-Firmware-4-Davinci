##Da Vinci Firmware based on Repetier (0.92)
============================

This firmware is based on the popular repetier firmware and modified to work with first generation Da Vinci 1.0, 2.0 single fan and 2.0.

***
###Support for 1.0A is implemented and need feedback 

The board can be easily exposed by removing the back panel of the printer secured by two torx screws.  Supported boards have a jumper labeled JP1.  Unsupported second generation boards have a jumper labeled J37. More info can be found on the [Voltivo forum](http://voltivo.com/forum/davinci-peersupport/340-new-kind-of-mainboard-no-j1-erase-jumper).
***

Here are just a few of the benifits of using this firmware:

* It works with host software such as [repetier host](http://repetier.com) and [OctoPrint](http://octoprint.org/) giving you full control of your hardware.
* It works stand alone if you use a WIFI SD Card. 
* It allows the use of clear ABS (by disabling optical sensors), as well as other arbitrary filament brands/types as temperatures can be controlled freely and there is no requirement for chiped cartridges. 

You can find more info on the [Voltivo forum](http://voltivo.com/forum/davinci-firmware).     

The current firmware is based on [repetier Firmware](https://github.com/repetier/Repetier-Firmware) 0.92 : [bgm370 Da Vinci 1.0 FW](https://github.com/bgm370/Repetier-Firmware)    
Sources are [here](https://github.com/luc-github/Repetier-Firmware-0.92)   
The previous version (based on repetier v0.91) can be found [here](https://github.com/luc-github/Repetier-Firmware)   

***
##Current Status
####Alpha 1 - ready for testing

***
##Installation
1. With the machine off remove the back panel and short the jumper JP1.  Some Boards do not have jumper pins exposed but can still be shorted with a conductive wire.
2. Turn the machine on and wait a few seconds then turn it off again.  The machine will have been flashed removing the current stock firmware and allowing it to be detected as a normal arduino DUE. NOTE: Windows users may need to install drivers to detect the board.  Consult the Voltivo forums.
3. Use an arduino IDE supporting arduino DUE, [version 1.5.8+](http://arduino.cc/en/Main/Software#toc3)    
4. Update arduino files (variants.cpp and USBcore.cpp) with the ones prensent in src\ArduinoDUE\AdditionalArduinoFiles\1.5.8.  NOTE: You do not need to compile arduino from source these files are in the arduino directory structure.  On Mac you will need to right click on the Arduino.app to Show Package Contents.    
5. Open the project file named repetier.ino located in src\ArduinoDUE\Repetier directory in the arduino IDE. 
6. Modify the DAVINCI define in Configuration.h file to match your targeted Da Vinci.  See below.
7. Under the tools menu select the board type as Arduino DUE (Native USB Port) and the proper port you have connected to the printer.  NOTE: You can usually find this out by looking at the tools -> port menu both before and after plugging in the printer to your computer's USB.
8. Press the usual arduino compile and upload button.
If done correctly you will see the arduino sketch compile successfully and output in the log showing the upload status.
9. Once flash is done <B>do not forget to send G-Code M502 then M500 </B>from repetier's Print Panel tab <B>or from the printer menu "Settings/Load Fail-Safe"</B> and accept to save the new eeprom settings. 
10. When update is complete <B>you must calibrate your bed height!</B>  
You can do so by homeing all axis, turning off the printer and manually adjusting the bed leveling screws until the glass bed is just under the nozzle at each end of the bed.  While the printer is powered off you can move the print head by hand and slide a piece of paper over the glass bed and under the nozzle with a slight pull on the paper from the nozzle.
11. Next you can calibrate your filament as usual.

For information on upgrading from or reverting to stock FW and other procedures please check [Da Vinci Voltivo forum](http://voltivo.com/forum/davinci).    

Do not forget to modify the configuration.h to match your targeted Da Vinci: 1.0, 2.0 SF or 2.0.   
for basic installation just change :   
'<code>#define DAVINCI 1 // "1" For DAVINCI 1.0, "2" For DAVINCI 2.0 with 1 FAN, "3" For DAVINCI 2.0 with 2 FANS</code>'    

  1 for DaVinci 1.0 (1Fan, 1 Extruder)   
  2 for DaVinci 2.0 SF (1Fan, 2 Extruders)   
  3 for DaVinci 2.0  (2Fans, 2 Extruders)   

For some boards that have a heatbed sensor with a slow response time and generate a defect temperature :   
Set WARMUP_BED_ON_INIT to 1, it will preheat bed if it is necessary to increase sensor response time when starting printer if a defect is detected, if it is a real sensor issue - it will stop after 10 seconds and generate a temperature defect error. 

Support for 1.0A:  need to change <CODE>#define MODEL 0</CODE>  to  <CODE>#define MODEL 1</CODE>

To repurpose the main Extruder cooling fan to be controlled VIA G-Code instructions M106/M107:   
Set REPURPOSE_FAN_TO_COOL_EXTRUSIONS to 1, do not forget to add a fan with power source to cool extruder permanently if you use this option.     

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

