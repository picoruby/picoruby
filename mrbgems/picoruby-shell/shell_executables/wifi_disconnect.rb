# wifi_disconnect.rb
#   Disconnect from the current WiFi network

begin
  require 'cyw43'
rescue LoadError
  puts "CYW43 not available"
  return
end

unless CYW43.initialized?
  puts "CYW43 not initialized"
  return
end

unless CYW43.link_connected?
  puts "WiFi not connected"
  return
end

puts "Disconnecting from WiFi..."
if CYW43.disconnect
  puts "Disconnected successfully"
  puts "DHCP address released: #{!CYW43.dhcp_supplied?}"
  puts "Link status: #{CYW43.link_connected? ? 'connected' : 'disconnected'}"
else
  puts "Failed to disconnect"
end
