#Module ESP8266 for usage on DaVinci
##Description      
Thanks to @disneysw for bringing this module idea and basic code    

Still on going , check TODO    
...   
TBD   

##Hardware connection     
...    
TBD      

##Flash the Module    
*tools:    
     --flash tool : https://github.com/themadinventor/esptool   
     --lualoader : http://benlo.com/esp8266/    
     --ESPlorer : http://esp8266.ru/esplorer/    
     --esp flasher: https://github.com/nodemcu/nodemcu-flasher 
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
...    
TBD    
    
NB: need to use format command before copy files    

##Wifi connection    
*Access point / Client station    
*DHCP/Static IP    
...    
TBD
    
##Configuration   
From web:   
<IMG SRC=Capture.PNG>    
    
##Commands    
*from module to printer    
    -M800 S1 , restart module done need a wifi/activity restart     
    -M801 [Message], Error message from module    
    -M802 [Message], Status message from module       
    -M804 [AP/STATION,SSID,DHC/STATIC,IP,MASK,GW,STATUS], ]Module configuration without password    
          STATUS follows : 0: STATION_IDLE, 1: STATION_CONNECTING, 2: STATION_WRONG_PASSWORD, 3: STATION_NO_AP_FOUND, 4: STATION_CONNECT_FAIL, 5: STATION_GOT_IP   
    ...    
    TBD    
        
*from host to printer    
    -M803 [IP, AP,SSID, Password....], ]Module configuration settings to be used  
    -M805 query to get M804 informations
    ...    
    TBD        
        
*from printer to module    
    -check scripts    
    ...    
    TBD    
    
##Scripts/files    
*init.lua, setup default baud rate, more TBD    
*datasave, configuration file    
*connect.lc, setup wifi according  configuration file (datasave), error if no file, currently no automatic generation out of web UI    
*page.tpl, UI template for configuration     
*webconfig.lc, web server to help to generate the  configuration file (datasave)     
*bridge.lua, transparent usb/wifi bridge    
*setup.lc, __not done yet__, to compile lua to lc and generate default configuration file    
*config.lc , __not done yet__,script to pass all parameters by command line if not wifi    
*activity.lc , __not done yet__, to launch bridge.lc or front end.lc according parrameters    
*status.lc , to get all information on request
*frontend.lc  , __not done yet__, to show printer information on web if not in bridge mode    
...    
TBD    

##TODO    
*setup.lua script to convert lua to lc to save space and generate default configuration file    
*config.lua script to pass all parameters by command line if not wifi   
*activity.lua to launch bridge.lc or front end.lc according parameters   
*allow bridge.lc to be stopped using GPIO2 pin and  free printer pin (EEPROM 1 pin ?)    
*frontend.lua to show printer information on web if not in bridge mode    
*repetier Fw connection   
*M803 command
*M805 command or M804 S1 for query configuration TBD
...   
more to come   
 
