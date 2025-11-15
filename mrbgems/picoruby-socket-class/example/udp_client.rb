require 'socket'

HOST = '0.0.0.0'
PORT = 5000

sock = UDPSocket.new
sock.connect(HOST, PORT)

sock.send("Hello, UDP server!", 0)

begin
  data = sock.recv(1024)
  puts "Received: #{data}"
rescue => e
  puts "No reply: #{e}"
end

sock.close
