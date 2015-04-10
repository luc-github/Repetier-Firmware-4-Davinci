--status.lua,  AP/Client(real),SSID(config),DHCP/Static(config),IP(real),Mask(real),Gateway(real),connection status (real)
--connection values: 0: STATION_IDLE, 1: STATION_CONNECTING, 2: STATION_WRONG_PASSWORD, 3: STATION_NO_AP_FOUND, 4: STATION_CONNECT_FAIL, 5: STATION_GOT_IP
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
--try to open configuration file
d,drf = pcall(get_data,data_configuration)
--if failed or error then use default values
 if (drf==0 or d~=true ) then
    --send error message
    print("M801 Error - no config file")
    --SSID
    data_configuration[2]="none?"
    --DHCP/Static
    data_configuration[4]="0?"
end
--get current mode
local smode = wifi.getmode()
local ip="0.0.0.0"
local mymask="0.0.0.0"
local gw="0.0.0.0"
local status = 0
--if station mode
if smode==1 then
    data_configuration[1]="2"
    ip,mymask,gw = wifi.sta.getip()
    --check if have ip 
    if ip==nil then
        ip="0.0.0.0"
        mymask="0.0.0.0"
        gw="0.0.0.0"
    end
    status = wifi.sta.status()
--else it is an AP
else
     data_configuration[1]="1"
     ip,mymask,gw = wifi.sta.getip()
    --check if have ip 
     if ip==nil then
        ip="0.0.0.0"
        mymask="0.0.0.0"
        gw="0.0.0.0"
    end
   end
print ( string.format("M804 %s,%s,%s,%s,%s,%s,%u", data_configuration[1],data_configuration[2],data_configuration[4],ip,mymask,gw,status))


