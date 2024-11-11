unless ENV['WIFI_MODULE'] == "cwy43"
  return
end

puts "Connecting to WiFi..."
system "wifi_connect --check-auto-connect"
