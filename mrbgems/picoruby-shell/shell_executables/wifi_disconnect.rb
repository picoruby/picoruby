# wifi_disconnect.rb
#   Disconnect from the current WiFi network

begin
  require 'network'
rescue LoadError
  puts "Network::WiFi not available"
  return
end

unless Network::WiFi.initialized?
  puts "Network::WiFi not initialized"
  return
end

unless Network::WiFi.link_connected?
  puts "WiFi not connected"
  return
end

puts "Disconnecting from WiFi..."
if Network::WiFi.disconnect
  puts "Disconnected successfully"
  puts "DHCP address released: #{!Network::WiFi.dhcp_supplied?}"
  puts "Link status: #{Network::WiFi.link_connected? ? 'connected' : 'disconnected'}"
else
  puts "Failed to disconnect"
end
