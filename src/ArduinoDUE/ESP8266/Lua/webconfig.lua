--webconfig.lua, a web server for web configuration
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

--change the special char coding form of the query to their original value
function url_decode(str)
    str = string.gsub (str,"%%(%x%x)",function(h) return string.char(tonumber(h,16)) end)
    return str
end
 
 --lets start the web server
srv=net.createServer(net.TCP)
srv:listen(80,function(conn)
     --if any request is received, with or without parameters
    conn:on("receive",function(client,request)
        --buffer
        local buf = ""
        --configuration table
        local data_configuration={ "","","","","","","",""}
        --variable to save space as used several times
        local scheck="checked"
        --flag for default values
        local default=1
        local _,_,method,path,vars = string.find(request,"([A-Z]+) (.+)?(.+) HTTP")
        if(method == nil)then
            _,_,method,path = string.find(request,"([A-Z]+) (.+) HTTP")
        end
        local _GET = {}
        --request has paremeters
        if (vars ~= nil)then
            --remove the spaces coded as + , because not suposed to have any space
            vars = string.gsub(vars,"+","")
            --assign a variable to each value received
            for k,v in string.gmatch(vars,"(%w+)=([%w.%%]+)&*") do
                f,vrf = pcall(url_decode,v)
                _GET[k] = vrf
            end
            --if restart is requested
            if  (_GET.SAVE == "RESTART") then
                --send message to printer
                print("M800 S1")
                --restart module
                node.restart()
            end;
            --if apply is requested
            if (_GET.SAVE == "Apply") then
                --we are not using default values
                default=0
                --assigne all request parameters to configuration parameters
                --Connection mode : Access point or client station
                data_configuration[1]=_GET.APP
                --SSID
                data_configuration[2] = _GET.SSID
                --Password for AP
                data_configuration[3] = _GET.PASSWORD
                --DHCP or Static IP
                data_configuration[4] = _GET.DHCP
                --IP address for static address
                data_configuration[5] = _GET.IPADDRESS
                --Mask for static address
                data_configuration[6] =_GET.MASK
                --Gateway for static address
                data_configuration[7] =_GET.GATEWAY
                --Usage :Bridge or Front end
                data_configuration[8] = _GET.BRIDGE
                --save parameters to configuration file
                file.open("datasave","w+")
                for key,value in pairs(data_configuration) do 
                    file.writeline(value)
                end
                file.flush()
                file.close()
            end
        end
        --use default parameters because no parameters in query or cancel is in request
        if  default==1 then
            --try to open configuration file
            d,drf = pcall(get_data,data_configuration)
            --if failed or error then use default values
            if (drf==0 or d~=true ) then
                --default  connection mode is AP
                data_configuration[1] = "1"
                --default SSID 
                data_configuration[2] = "ESP8266"
                --default password for SSID 
                data_configuration[3] = "12345678"
                --default  is static IP because more easy to find it
                data_configuration[4] = "2"
                --IP of AP 
                data_configuration[5] = "192.168.1.1"
                --Mask of AP 
                data_configuration[6] = "255.255.255.0"
                --Gateway of AP 
                data_configuration[7] = "192.168.1.1"
                --default usage is bridge
                data_configuration[8] ="1"
            end
        end
        --open template for  HTML page
        if file.open("page.tpl","r")~=nil then
            --read all template lines, template has minmum lines to decrease number of loop and then actions 
            buf = file.readline()
            while buf~=nil do
                --if we have a $ it means there are variables to change, if not no need to proceed so it save execution load
                if string.find(buf,"$")~=nil then
                    --check station radio if station is in config
                    if (data_configuration[1]=="2") then
                        buf = string.gsub(buf,"$APP_CHK","")
                        buf = string.gsub(buf,"$STATION_CHK",scheck)
                     --else check access point radio
                    else
                        buf = string.gsub(buf,"$APP_CHK",scheck)
                        buf = string.gsub(buf,"$STATION_CHK","")
                    end
                    --SSID
                    buf = string.gsub(buf,"$SSID",data_configuration[2])
                    --Password
                    buf = string.gsub(buf,"$PWD",data_configuration[3])
                    --check static IP radio if static IP is in config, make extra parameters visible
                    if (data_configuration[4]=="2") then
                        buf = string.gsub(buf,"$DHCP_CHK","")
                        buf = string.gsub(buf,"$STATIC_CHK",scheck)
                        buf = string.gsub(buf,"$STATIC_VISIBLE","")
                     --else check DHCP radio, hide extra parameters 
                    else
                        buf = string.gsub(buf,"$DHCP_CHK",scheck)
                        buf = string.gsub(buf,"$STATIC_CHK","")
                        buf = string.gsub(buf,"$STATIC_VISIBLE","none")
                    end
                    --IP address for static address, not visible if DHCP selected
                    buf = string.gsub(buf,"$IP_ADDRESS",data_configuration[5])
                    --Mask for static address, not visible if DHCP selected
                    buf = string.gsub(buf,"$MASK",data_configuration[6])
                    --Gateway for static address, not visible if DHCP selected
                    buf = string.gsub(buf,"$GATEWAY",data_configuration[7])
                     --check front end radio if front end is in config,
                    if (data_configuration[8]=="2") then
                        buf = string.gsub(buf,"$BRIDGE_CHK","")
                        buf = string.gsub(buf,"$FRONT_END_CHK", scheck)
                        --else check bridge radio
                    else
                        buf = string.gsub(buf,"$BRIDGE_CHK", scheck)
                        buf = string.gsub(buf,"$FRONT_END_CHK","")
                    end
                    --display a message according action
                    --if apply was in request
                    if  default==0  then
                        buf = string.gsub(buf,"$MSG","Changes applied")
                    else
                        -- if no configuration file or cancel is in request
                        buf = string.gsub(buf,"$MSG","Configuration loaded")
                    end
                end
                --send line to client
                client:send(buf)
                --read next template line
                buf = file.readline()
            end
            file.close()
        end
        --we are done close connection
        client:close()
        collectgarbage()
    end)
end)
