require 'socket'

# TLS client example for embedded platforms (RP2040, ESP32)
# This example shows how to use CA certificates on embedded systems

HOST = 'api.example.com'
PORT = 443

puts "TLS client for embedded platforms"
puts "Connecting to #{HOST}:#{PORT}..."

begin
  # Create SSL context
  ctx = SSLContext.new

  # For embedded platforms, CA certificate must be loaded from memory
  # Example: CA certificate embedded in flash memory
  #
  # In your code, define the CA certificate like this:
  #   CA_CERT = <<~CERT
  #     -----BEGIN CERTIFICATE-----
  #     MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
  #     ...
  #     -----END CERTIFICATE-----
  #   CERT
  #
  # Then get its address and size:
  #   ca_addr = CA_CERT.object_id
  #   ca_size = CA_CERT.bytesize
  #   ctx.set_ca(ca_addr, ca_size)

  # For testing without certificate validation:
  ctx.verify_mode = SSLContext::VERIFY_NONE
  puts "Warning: Certificate verification disabled"

  # For production with certificate validation:
  # ctx.verify_mode = SSLContext::VERIFY_PEER
  # ctx.set_ca(ca_addr, ca_size)

  # Create TCP connection
  tcp_sock = TCPSocket.new(HOST, PORT)
  puts "TCP connected"

  # Upgrade to TLS
  ssl_sock = SSLSocket.new(tcp_sock, ctx)
  ssl_sock.connect
  puts "TLS handshake complete"

  # Send request
  request = "GET /api/v1/status HTTP/1.1\r\n"
  request += "Host: #{HOST}\r\n"
  request += "User-Agent: PicoRuby-TLS/1.0\r\n"
  request += "Accept: application/json\r\n"
  request += "Connection: close\r\n"
  request += "\r\n"

  bytes_sent = ssl_sock.write(request)
  puts "Sent #{bytes_sent} bytes"

  # Read response headers
  response = ""
  5.times do
    chunk = ssl_sock.read(512)
    break unless chunk
    response += chunk
    break if response.include?("\r\n\r\n")
  end

  # Parse and display response
  lines = response.split("\r\n")
  puts "Status: #{lines[0]}"
  puts "Body preview: #{response.split("\r\n\r\n")[1]&.slice(0, 100)}"

  ssl_sock.close
  puts "Connection closed"

rescue => e
  puts "Error: #{e.class}: #{e.message}"
end
