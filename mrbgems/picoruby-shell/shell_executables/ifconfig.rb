# ifconfig - display network interface configuration

begin
  require 'cyw43'
rescue LoadError
  puts "Network interface not available"
  return
end

unless CYW43.initialized?
  puts "CYW43 not initialized"
  return
end

puts "wlan0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>"
puts "        Link: WiFi"

if CYW43.link_connected?
  status = "UP"
  link_status = case CYW43.tcpip_link_status
  when CYW43::LINK_UP
    "LINK_UP"
  when CYW43::LINK_NOIP
    "LINK_NOIP"
  when CYW43::LINK_JOIN
    "LINK_JOIN"
  when CYW43::LINK_DOWN
    "LINK_DOWN"
  when CYW43::LINK_FAIL
    "LINK_FAIL"
  when CYW43::LINK_NONET
    "LINK_NONET"
  when CYW43::LINK_BADAUTH
    "LINK_BADAUTH"
  else
    "UNKNOWN"
  end
  puts "        Status: #{status} (#{link_status})"

  if CYW43.dhcp_supplied?
    puts "        DHCP: Active"
  else
    puts "        DHCP: Inactive"
  end
else
  puts "        Status: DOWN"
end

puts
