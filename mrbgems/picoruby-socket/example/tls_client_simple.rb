require 'socket'

# Simple TLS client example
# This example shows the minimum code needed for a TLS connection

HOST = 'www.example.com'
PORT = 443

puts "Connecting to #{HOST}:#{PORT} via TLS..."

# Create SSL context and TCP socket
ctx = SSLContext.new
ctx.verify_mode = SSLContext::VERIFY_NONE  # Skip certificate verification

tcp = TCPSocket.new(HOST, PORT)
ssl = SSLSocket.new(tcp, ctx)
ssl.connect

puts "Connected!"

# Send simple HTTP GET request
request = "GET / HTTP/1.1\r\n"
request += "Host: #{HOST}\r\n"
request += "Connection: close\r\n"
request += "\r\n"
ssl.write(request)

# Read first line of response
response = ssl.read(100)
puts "Response: #{response[0..50]}..."

ssl.close
