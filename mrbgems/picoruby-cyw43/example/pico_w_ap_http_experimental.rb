require "cyw43"
require "socket"

# Experimental AP + HTTP sample for Pico W / Pico 2 W.
# This is intended for hardware confirmation only.
# AP mode now starts a minimal DHCP server for the default 192.168.4.0/24 network.
# If a client still cannot reach the page, future AP status/IP control work may still be needed.

CYW43.init("JP")
CYW43.enable_ap_mode("PICO-TIMER", "12345678")

ap_ip = CYW43.ap_ipv4_address
puts "AP started"
puts "AP IP: #{ap_ip || 'unassigned'}"
puts "Open: http://#{ap_ip}/" if ap_ip

server = TCPServer.new("0.0.0.0", 80)
puts "HTTP server listening on port 80"

response = "Hello from PicoRuby AP mode\n"

loop do
  client = server.accept

  begin
    request_line = client.gets
    puts "Request: #{request_line.inspect}" if request_line

    client.write "HTTP/1.1 200 OK\r\n"
    client.write "Content-Type: text/plain\r\n"
    client.write "Content-Length: #{response.bytesize}\r\n"
    client.write "Connection: close\r\n"
    client.write "\r\n"
    client.write response
  rescue => e
    puts "HTTP sample error: #{e.class}: #{e.message}"
  ensure
    client.close
  end
end
