--connect.lua, setup wifi according configuration file
-- -------------------------------

--File datasave has one line for each parameter (more or less lines means error)
local get_data = function (connection_data)
    --check if file can be opened
    if file.open("datasave","r")==nil then return 0 end
    for i = 1,8,1 do
        --if any error reading line
        if file.readline() == nil then 
        file.close()
        return 0 
        end
    end
    --if one more, it means file is not correct
    if file.readline() ~= nil then return 0 end
    --if we are here it means so far so good, so rewind to begining
    file.seek("set")
    --read each parameter, one by one
    for i = 1,8,1 do
        connection_data[i] = string.gsub(file.readline(),"\n","")
    end
    file.close()
    --no problem so far 
    return 1
end

--configuration table
local data_configuration={ "","","","","","","",""}
--send status message
print("M802 Starting Wifi")
--try to open configuration file
d,drf = pcall(get_data,data_configuration)
--if failed or error then use default values
 if (drf==0 or d~=true ) then
    --send error message
    print("M801 Error - no config file")
else
    --if access point
    if data_configuration[1]=="1"then
        --static IP
        if  data_configuration[4]=="2" then
            --IP is static so it is central AP
            wifi.setmode(wifi.SOFTAP)
            --AP SSID and Password
            wifi.ap.config({ssid=data_configuration[2],pwd=data_configuration[3]})
            --IP/Mask/Gateway
            wifi.ap.setip(wifi.sta.setip({ip=data_configuration[5],netmask=data_configuration[6], gateway=data_configuration[7] }))
        --DHCP for AP
        else
            --AP and client at once
            wifi.setmode(wifi.STATIONAP)
            --AP SSID and Password
            wifi.ap.config({ssid=data_configuration[2],pwd=data_configuration[3]})
            wifi.ap.getip()
        end
    --else if client station
    elseif data_configuration[1]=="2"then
        --set client station
        wifi.setmode(wifi.STATION)
         --AP SSID and Password
        wifi.sta.config(data_configuration[2],data_configuration[3])
        --set autoconnect to true , should not be necessary but
        wifi.sta.autoconnect(1)
        --if static IP
        if  data_configuration[4]=="2" then
             --IP/Mask/Gateway
            wifi.sta.setip({ip=data_configuration[5],netmask=data_configuration[6], gateway=data_configuration[7] })
        else
            wifi.sta.getip()
        end
    end 
    --if no AP neither Station then do nothing
end
