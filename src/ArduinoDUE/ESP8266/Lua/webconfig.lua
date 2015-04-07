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

function url_decode(str)
str = string.gsub (str,"%%(%x%x)",function(h) return string.char(tonumber(h,16)) end)
return str
end
 
srv=net.createServer(net.TCP)
srv:listen(80,function(conn)
conn:on("receive",function(client,request)
local buf = ""
local data_configuration={ "","","","","","","",""}
local scheck="checked"
local default=1
local _,_,method,path,vars = string.find(request,"([A-Z]+) (.+)?(.+) HTTP")
if(method == nil)then
_,_,method,path = string.find(request,"([A-Z]+) (.+) HTTP")
end
local _GET = {}
if (vars ~= nil)then
vars = string.gsub(vars,"+","")
for k,v in string.gmatch(vars,"(%w+)=([%w.%%]+)&*") do
f,vrf = pcall(url_decode,v)
_GET[k] = vrf
end
if  (_GET.SAVE == "RESTART") then
print("M800 S1")
node.restart()
end;
if (_GET.SAVE == "Apply") then
default=0
data_configuration[1]=_GET.APP
data_configuration[2] = _GET.SSID
data_configuration[3] = _GET.PASSWORD
data_configuration[4] = _GET.DHCP
data_configuration[5] = _GET.IPADDRESS
data_configuration[6] =_GET.MASK
data_configuration[7] =_GET.GATEWAY
data_configuration[8] = _GET.BRIDGE
file.open("datasave","w+")
for key,value in pairs(data_configuration) do 
file.writeline(value)
end
file.flush()
file.close()
end
end
if  default==1 then
d,drf = pcall(get_data,data_configuration)
 if (drf==0 or d~=true ) then
data_configuration[1] = "1"
data_configuration[2] = "ESP8266"
data_configuration[3] = "12345678"
data_configuration[4] = "2"
data_configuration[5] = "192.168.1.1"
data_configuration[6] = "255.255.255.0"
data_configuration[7] = "192.168.1.1"
data_configuration[8] ="1"
end
end
if file.open("page.tpl","r")~=nil then
buf = file.readline()
while buf~=nil do
if string.find(buf,"$")~=nil then
if (data_configuration[1]=="2") then
buf = string.gsub(buf,"$APP_CHK","")
buf = string.gsub(buf,"$STATION_CHK",scheck)
else
buf = string.gsub(buf,"$APP_CHK",scheck)
buf = string.gsub(buf,"$STATION_CHK","")
end
buf = string.gsub(buf,"$SSID",data_configuration[2])
buf = string.gsub(buf,"$PWD",data_configuration[3])
if (data_configuration[4]=="2") then
buf = string.gsub(buf,"$DHCP_CHK","")
buf = string.gsub(buf,"$STATIC_CHK",scheck)
buf = string.gsub(buf,"$STATIC_VISIBLE","")
else
buf = string.gsub(buf,"$DHCP_CHK",scheck)
buf = string.gsub(buf,"$STATIC_CHK","")
buf = string.gsub(buf,"$STATIC_VISIBLE","none")
end
buf = string.gsub(buf,"$IP_ADDRESS",data_configuration[5])
buf = string.gsub(buf,"$MASK",data_configuration[6])
buf = string.gsub(buf,"$GATEWAY",data_configuration[7])
if (data_configuration[8]=="2") then
buf = string.gsub(buf,"$BRIDGE_CHK","")
buf = string.gsub(buf,"$FRONT_END_CHK", scheck)
else
buf = string.gsub(buf,"$BRIDGE_CHK", scheck)
buf = string.gsub(buf,"$FRONT_END_CHK","")
end
if  default==0  then
buf = string.gsub(buf,"$MSG","Changes applied")
else
buf = string.gsub(buf,"$MSG","Configuration loaded")
end
end
client:send(buf)
buf = file.readline()
end
file.close()
end
client:close()
collectgarbage()
end)
end)
