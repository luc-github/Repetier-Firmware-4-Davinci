<html><head><title>Wifi Configuration</title><script>function Restart(){if (confirm("Do you confirm restart ?"))window.open(window.location.pathname  + "?SAVE=RESTART" ,"_self");}</script>
</head><body><H1>Configuration</H1><HR><FORM><H3>Mode</H3>
<INPUT type="radio" name="APP" value="1" $APP_CHK>Access Point<INPUT type="radio" name="APP" value="2" $STATION_CHK>Station
<HR> <H3>Connection</H3><TABLE><TR><TD>SSID:</TD><TD><INPUT type="text" name="SSID" value="$SSID"></TD></TR><TR><TD>Password:</TD>
<TD><INPUT type="password" name="PASSWORD" value="$PWD"></TD></TR></TABLE><HR> <H3>IP address</H3>
<INPUT type="radio" Name="DHCP" value="1" onclick="document.getElementById('IPADDRESS').style.display  = 'none';"  $DHCP_CHK>DHCP
<INPUT type="radio" Name="DHCP" value="2" onclick="document.getElementById('IPADDRESS').style.display = '';" $STATIC_CHK>Static IP
<TABLE  ID=IPADDRESS style='display:$STATIC_VISIBLE'><TR><TD><HR>IP:<TD><HR><INPUT type="text" name="IPADDRESS" value="$IP_ADDRESS"></TD>
</TR><TR><TD>Mask:</TD><TD><INPUT type="text" name="MASK" value="$MASK"></TD></TR><TR><TD>Gateway:</TD><TD><INPUT type="text" name="GATEWAY" value ="$GATEWAY">
</TD></TR></TABLE><HR><H3>Usage</H3><INPUT type="radio" name="BRIDGE" value="1" $BRIDGE_CHK>Bridge
<INPUT type="radio" name="BRIDGE" value="2"  $FRONT_END_CHK>Front End<HR> <INPUT type="submit" Name="SAVE" value="Apply"><INPUT type="submit" Name="SAVE" value="Cancel">
 <INPUT type="button" value="Restart using configuration"  onclick="Restart();"></FORM>
 <HR>$MSG</body></html>
