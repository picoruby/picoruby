require 'socket'

HOST = ARGV[0] || 'www.example.com'
PORT = 443

puts "Starting TLS client to connect to #{HOST}:#{PORT}"

begin
  # Create SSL context
  ctx = SSLContext.new
  ctx.verify_mode = SSLContext::VERIFY_PEER

  # Set CA certificate if needed (for embedded platforms)
  # ctx.set_ca(ca_addr, ca_size)

  # Create TCP socket
  # Note: The TCP socket is used only to extract hostname and port.
  # SSLSocket will create its own TCP connection internally.
  tcp_sock = TCPSocket.new(HOST, PORT)
  puts "TCP connection established to #{HOST}:#{PORT}"

  # Wrap with SSL (this closes tcp_sock and creates a new connection)
  ssl_sock = SSLSocket.new(tcp_sock, ctx)
  ssl_sock.connect
  puts "TLS handshake completed"

  # Send HTTPS request
  request = "GET / HTTP/1.1\r\n"
  request += "Host: #{HOST}\r\n"
  request += "Connection: close\r\n"
  request += "\r\n"

  ssl_sock.write(request)
  puts "Sent HTTP request"

  # Read response
  response = ""
  loop do
    chunk = ssl_sock.read(1024)
    break unless chunk
    response += chunk
    break if response.include?("\r\n\r\n")
  end

  puts "Received response:"
  puts response.split("\r\n\r\n")[0]

  ssl_sock.close
  puts "Connection closed"
rescue => e
  puts "Error: #{e.message}"
end
