##Da Vinci Firmware based on Repetier (0.92.10) Beta   
============================

[![Join the chat at https://gitter.im/luc-github/Repetier-Firmware-0.92](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/luc-github/Repetier-Firmware-0.92?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)    

Build Status: [![Build Status](https://travis-ci.org/luc-github/Repetier-Firmware-0.92.svg?branch=master)](https://travis-ci.org/luc-github/Repetier-Firmware-0.92)    
    
      

This firmware is based on the popular repetier firmware Da Vinci 1.0/A, 2.0 single fan, 2.0/A dual fans and also AiO (NB:scanner function is not supported so AiO will work like an 1.0A)   
If you change the board, currently DUE based are supported with RADDS, as well as Graphical screen and LCD with encoder, there are some sample configuration files provided for RADDS/DUE/GLCD using 1/128 step drivers.

YOU MIGHT DAMAGE YOUR PRINTER OR VOID YOUR WARRANTY, DO IT ON YOUR OWN RISK. When it is possible on 1.0/2.0, currently on 1.0A/2.0A and AiO there is no way to revert to stock fw so be sure of what you are doing.

***
AiO scanner support is present in FW but scanner software support is currently basic, [horus](https://github.com/bqlabs/horus) is a good candidat, feel free to help [here](https://github.com/luc-github/Repetier-Firmware-0.92/issues/156)  

The board can be easily exposed by removing the back panel of the printer secured by two torx screws.  Supported boards have a jumper labeled JP1, second generation boards have a jumper labeled J37. More info can be found on the [Voltivo forum](http://voltivo.com/forum/davinci-peersupport/340-new-kind-of-mainboard-no-j1-erase-jumper).
***

Here are just a few of the benefits of using this firmware:

* It works with host software such as [repetier host](http://repetier.com) and [OctoPrint](http://octoprint.org/) giving you full control of your hardware.
* It works stand alone if you use a WIFI SD Card. 
* It allows the use of clear ABS (by disabling optical sensors), as well as other arbitrary filament brands/types as temperatures can be controlled freely and there is no requirement for chiped cartridges. 

You can find more info on the [Voltivo forum](http://voltivo.com/forum/davinci-firmware).  

The current firmware is based on [repetier Firmware](https://github.com/repetier/Repetier-Firmware) 0.92 : [bgm370 Da Vinci 1.0 FW](https://github.com/bgm370/Repetier-Firmware)    
Sources are [here](https://github.com/luc-github/Repetier-Firmware-0.92)   
The previous version (based on repetier v0.91) can be found [here](https://github.com/luc-github/Repetier-Firmware)   

***
##Current Status
####Beta - so far so good

***
##Installation
1. With the machine off remove the back panel and short the jumper JP1 or J37 depending on model.  Some Boards do not have jumper pins exposed but can still be shorted with a conductive wire.
2. Turn the machine on and wait a few seconds then turn it off again.  The machine will have been flashed removing the current stock firmware and allowing it to be detected as a normal arduino DUE. NOTE: Windows users may need to install drivers to detect the board.  Consult the Voltivo forums.   
Note : points 1 and 2 are only needed to wipe the stock fw or a corrupted fw, for update they are not necessary.
Note 2: remove the jumper before flashing if still there
3. Use an arduino IDE supporting arduino DUE, [1.6.9](http://arduino.cc/en/Main/OldSoftwareReleases) with Due 1.6.8 module from board manager [FAQ#166](https://github.com/luc-github/Repetier-Firmware-0.92/issues/166).
4. Update variants.cpp arduino file with the one present in src\ArduinoDUE\AdditionalArduinoFile according your IDE version   
NOTE: You do not need to compile arduino from source these files are in the arduino directory structure [FAQ#114](https://github.com/luc-github/Repetier-Firmware-0.92/issues/114).    
5. Open the project file named repetier.ino located in src\ArduinoDUE\Repetier directory in the arduino IDE. 
6. Modify the DAVINCI define in Configuration.h file to match your targeted Da Vinci.  See below.
7. Under the tools menu select the board type as Arduino DUE (Native USB Port) and the proper port you have connected to the printer.  NOTE: You can usually find this out by looking at the tools -> port menu both before and after plugging in the printer to your computer's USB.
8. Press the usual arduino compile and upload button.
If done correctly you will see the arduino sketch compile successfully and output in the log showing the upload status.
9. Once flash is done : restart printer   
10. After printer restarted <B>do not forget to send G-Code M502 then M500 </B>from repetier's Print Panel tab <B>or from the printer menu "Settings/Load Fail-Safe"</B> and accept to save the new eeprom settings. 
11. When update is complete <B>you must calibrate your bed height!</B>Use manual bed leveling in menu
12. Next you can calibrate your filament as usual, and second extruder offset if you have.

For information on upgrading from or reverting to stock FW and other procedures please check [Da Vinci Voltivo forum](http://voltivo.com/forum/davinci).    
<h4>:warning:There is no known way to revert to stock FW on 1.0A/2.0A/AiO until today.</h4>     

Do not forget to modify the configuration.h to match your targeted Da Vinci: 1.0, 2.0 SF or 2.0.   
for basic installation just change :   
'<code>#define DAVINCI 1 // "1" For DAVINCI 1.0, "2" For DAVINCI 2.0 with 1 FAN, "3" For DAVINCI 2.0 with 2 FANS, 4 For AiO （no scanner)</code>'    
  0 for not Davinci board (like DUE/RADDS)    
  1 for DaVinci 1.0 (1Fan, 1 Extruder)   
  2 for DaVinci 2.0 SF (1Fan, 2 Extruders)   
  3 for DaVinci 2.0  (2Fans, 2 Extruders)    
  4 for DaVinci AiO 

Support for 1.0A and 2.0A:  need to change <CODE>#define MODEL 0</CODE>  to  <CODE>#define MODEL 1</CODE>

To repurpose the main Extruder cooling fan to be controlled VIA G-Code instructions M106/M107:   
Set REPURPOSE_FAN_TO_COOL_EXTRUSIONS to 1, do not forget to add a fan with power source to cool extruder permanently if you use this option.     

Another excellent tutorial for flashing and installation is here from Jake : http://voltivo.com/forum/davinci-peersupport/501-da-vinci-setup-guide-from-installation-to-wireless-printing but Arduino ide version described is not correct for latest firmware version, use the one mentioned above.   

Or a great video done by Daniel Gonos: https://www.youtube.com/watch?v=rjuCvlnpB7M but Arduino ide version described is not correct for latest firmware version, use the one mentioned above.   

***
##TODO or Questions ?
* [Check issue list](https://github.com/luc-github/Repetier-Firmware-0.92/issues)   
Do not ask help on repetier github they do not support this FW / printer - please use this [github for issues](https://github.com/luc-github/Repetier-Firmware-0.92/issues)

* [FAQ](https://github.com/luc-github/Repetier-Firmware-0.92/issues?utf8=%E2%9C%93&q=is%3Aclosed+label%3AFAQ+)    

* [Documentation](https://github.com/luc-github/Repetier-Firmware-0.92/wiki) TBD - feel free to help 

***
##Implemented
* 0.92.10 [Repetier](https://github.com/repetier/Repetier-Firmware) based   
* Standard GCODE commands   
* Single/Dual extruders support DaVinci 1.0/A, 2.0/A all generations, AiO but no scanner support because no application
* Single Fan / Dual fans support according printer configuration
* Repurpose of second fan usage to be controlled by M106/M107 commands on Da Vinci 2.0/A, 1.0/A need additional fan
* Sound and Lights management, including powersaving function (light can be managed remotely by GCODE)
* Cleaning Nozzle(s) by menu and by GCODE for 1.0, 2.0 and AiO
* Load / Unload filament by menu
* Filament Sensor support (auto loading / alert if no filament when printing)
* Auto Z-probe / average Z position calculation
* Manual Leveling
* Dripbox cleaning for 1.0/2.0
* Customized Menu UI with Advanced/Easy mode (switch in "Settings/Mode" or using Up key/Right key/ Ok key in same time)
* Loading FailSafe settings
* Emergency stop (Left key/Down key/Ok key  in same time)
* Increase extruder temperature range to 270C and bed to 130C
* Add temperature control on extruder to avoid movement if too cold
* Add fast switch (1/10/100mm) for manual position/extrusion
* Command for bed down
* Several fixes from original FW   
* Watchdog   
* Basic Wifi support for module ESP8266 (https://github.com/luc-github/ESP8266/blob/master/README.md#result-of-esp12e-on-davinci) 
* Customized thermistor tables for bed and extruder(s) as Davinci boards do not follow design of others 3D printer boards so standard tables do not work properly [check here](http://voltivo.com/forum/davinci-firmware/438-repetier-91-e3d-v6-extruder#3631)  
* Multilanguage at runtime (EN/FR/GE/IT/NL/SW) more to come if get help : check [here](https://github.com/luc-github/Repetier-Firmware-0.92/issues/123)   
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

##Donation:
Every support is welcome: [<img src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG_global.gif" border="0" alt="PayPal – The safer, easier way to pay online.">](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=VT5LV38N4U3VQ)    
Especially if need to buy new printer to add FW support.
