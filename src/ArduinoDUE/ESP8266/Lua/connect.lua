local get_data = function (connection_data)
if file.open("datasave","r")==nil then return 0 end
for i = 1,8,1 do
if file.readline() == nil then 
file.close()
return 0 
end
end
if file.readline() ~= nil then return 0 end
file.seek("set")
for i = 1,8,1 do
connection_data[i] = string.gsub(file.readline(),"\n","")
end
file.close()
return 1
end

local data_configuration={ "","","","","","","",""}
print("M802 Starting Wifi")
d,drf = pcall(get_data,data_configuration)
 if (drf==0 or d~=true ) then
print("M801 Error - no config file")
else
if data_configuration[1]=="1"then
if  data_configuration[4]=="2" then
wifi.setmode(wifi.SOFTAP)
wifi.ap.config({ssid=data_configuration[2],pwd=data_configuration[3]})
wifi.ap.setip(wifi.sta.setip({ip=data_configuration[5],netmask=data_configuration[6], gateway=data_configuration[7] }))
else
wifi.setmode(wifi.STATIONAP)
wifi.ap.config({ssid=data_configuration[2],pwd=data_configuration[3]})
wifi.ap.getip()
end
elseif data_configuration[1]=="2"then
wifi.setmode(wifi.STATION)
wifi.sta.config(data_configuration[2],data_configuration[3])
wifi.sta.autoconnect(1)
if  data_configuration[4]=="2" then
wifi.sta.setip({ip=data_configuration[5],netmask=data_configuration[6], gateway=data_configuration[7] })
else
wifi.sta.getip()
end
end
end
