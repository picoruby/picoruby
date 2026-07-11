# ifconfig - display network interface configuration

begin
  require 'network'
rescue LoadError
  puts "Network interface not available"
  return
end

unless Network::WiFi.initialized?
  puts "Network::WiFi not initialized"
  return
end

puts "Link: WiFi"

puts "  Station:"
if Network::WiFi.link_connected?
  puts "    Status: UP (#{Network::WiFi.tcpip_link_status_name})"
  if Network::WiFi.dhcp_supplied?
    puts "    DHCP: Active"
    puts "    IP Address: #{Network::WiFi.ipv4_address || 'unassigned'}"
    puts "    Netmask:    #{Network::WiFi.ipv4_netmask || 'unassigned'}"
    puts "    Gateway:    #{Network::WiFi.ipv4_gateway || 'unassigned'}"
  else
    puts "    DHCP: Inactive"
  end
else
  puts "    Status: DOWN"
end

puts "  Access Point:"
ap_ip = Network::WiFi.ap_ipv4_address
ap_netmask = Network::WiFi.ap_ipv4_netmask
ap_gateway = Network::WiFi.ap_ipv4_gateway
if ap_ip || ap_netmask || ap_gateway
  puts "    Status: UP"
  puts "    IP Address: #{ap_ip || 'unassigned'}"
  puts "    Netmask:    #{ap_netmask || 'unassigned'}"
  puts "    Gateway:    #{ap_gateway || 'unassigned'}"
else
  puts "    Status: DOWN"
end
