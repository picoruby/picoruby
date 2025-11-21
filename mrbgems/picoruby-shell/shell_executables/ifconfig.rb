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

puts "Link: WiFi"

if CYW43.link_connected?
  puts "  Status: UP (#{CYW43.tcpip_link_status_name})"
  if CYW43.dhcp_supplied?
    puts "  DHCP: Active"
    puts "  IP Address: #{CYW43.ipv4_address || 'unassigned'}"
    puts "  Netmask:    #{CYW43.ipv4_netmask || 'unassigned'}"
    puts "  Gateway:    #{CYW43.ipv4_gateway || 'unassigned'}"
  else
    puts "  DHCP: Inactive"
  end
else
  puts "  Status: DOWN"
end
