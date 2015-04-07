##Module ESP8266 for usage on DaVinci
Description:
Thanks to @disneysw for bringing this module idea and basic code

Still on going , check TODO
...
TBD

#Hardware connection :
TBD

#Flash the Module
*tools:
https://github.com/themadinventor/esptool
...
TBD

*Hardware connection :
...
TBD

*Latest binaries: 
https://github.com/nodemcu/nodemcu-firmware/releases/latest

*Current :
https://github.com/nodemcu/nodemcu-firmware/tree/0.9.6-dev_20150406

*How to:
1- flash module fw
2 - copy all files
3 - convert lua to lc files
....
TBD

NB: need to use format command before copy files

#Wifi connection
*Access point / Client station
*DHCP/Static IP
.....
TBD

#Configuration
From web:
<IMG SRC=Capture.PNG>

#Commands
*from module to printer
    -M800 S1 , restart module done need a wifi/activity restart
    -M801 [Message], Error message from module
    -M802 [Message], Status message from module
    -M804 [IP, AP,SSID,....], ]Module configuration without password
    ...TBD
    
*from host to printer
    -M803 [IP, AP,SSID, Password....], ]Module configuration settings to be used
    ..TBD
    
*from printer to module
    -check scripts
    ...TBD

#Scripts/files:
*init.lua, setup default baud rate, more TBD
*datasave, configuration file
*connect.lc, setup wifi according  configuration file (datasave), error if no file, currently no automatic generation out of web UI
*page.tpl, UI template for configuration 
*webconfig.lc, web server to help to generate the  configuration file (datasave)
*bridge.lua, transparent usb/wifi bridge
*setup.lc, __not done yet__, to compile lua to lc and generate default configuration file
*config.lc , __not done yet__,script to pass all parameters by command line if not wifi
*activity.lc , __not done yet__, to launch bridge.lc or front end.lc according parrameters
*status.lc , __not done yet__,to get all information on request
*frontend.lc  , __not done yet__, to show printer information on web if not in bridge mode
...
TBD

#TODO
*setup.lua script to convert lua to lc to save space and generate default configuration file
*config.lua script to pass all parameters by command line if not wifi
*activity.lua to launch bridge.lc or front end.lc according parrameters
*status.lua to get all information on request
*allow bridge.lc to be stopped using GPIO2 pin and  free printer pin (EEPROM 1 pin ?)
*frontend.lua to show printer information on web if not in bridge mode
*repetier Fw connection
...
more to come
 
