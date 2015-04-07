-- Transparent WiFi to UART Server
-- -------------------------------

-- Once started the only way to terminate this app is via a hw reset

-- Open a listening port on 8080
sv=net.createServer(net.TCP, 60)
global_c = nil
sv:listen(8080, function(c)

-- Loop around reading/writing data
if global_c~=nil then
global_c:close()
end
global_c=c
c:on("receive",function(sck,pl) uart.write(0,pl) end)
end)
uart.on("data",4, function(data)
if global_c~=nil then
global_c:send(data)
end
end, 0)


