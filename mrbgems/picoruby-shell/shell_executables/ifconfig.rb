# ifconfig - display network interface configuration

begin
  require 'network'
rescue LoadError
  puts "Network interface not available"
  return
end

# Which link this board has is decided in picoruby-network by
# RUBY_PLATFORM, and the two are not interchangeable: a Thread mesh has
# no SSID, no DHCP and no IPv4, so it cannot answer the WiFi questions
# below. Ask for whichever constant was defined rather than assuming.
# const_defined? would need picoruby-metaprog, which neither this gem nor
# picoruby-network declares; a rescue costs nothing and adds no dependency.
thread = begin
  Network::Thread
rescue NameError
  nil
end

if thread
  puts "Link: Thread"
  puts "  Role: #{thread.role}"
  if thread.attached?
    puts "  Status: UP"
    puts "  Mesh-local EID: #{thread.eid || 'unassigned'}"
    addresses = thread.addresses
    if addresses.empty?
      puts "  Addresses: none"
    else
      puts "  Addresses:"
      i = 0
      addresses_size = addresses.size
      while i < addresses_size
        puts "    #{addresses[i]}"
        i += 1
      end
    end
  else
    puts "  Status: DOWN"
  end
  return
end

wifi = begin
  Network::WiFi
rescue NameError
  nil
end

unless wifi
  puts "No network link on this platform"
  return
end

unless Network::WiFi.initialized?
  puts "Network::WiFi not initialized"
  return
end

puts "Link: WiFi"

if Network::WiFi.link_connected?
  puts "  Status: UP (#{Network::WiFi.tcpip_link_status_name})"
  if Network::WiFi.dhcp_supplied?
    puts "  DHCP: Active"
    puts "  IP Address: #{Network::WiFi.ipv4_address || 'unassigned'}"
    puts "  Netmask:    #{Network::WiFi.ipv4_netmask || 'unassigned'}"
    puts "  Gateway:    #{Network::WiFi.ipv4_gateway || 'unassigned'}"
  else
    puts "  DHCP: Inactive"
  end
else
  puts "  Status: DOWN"
end
