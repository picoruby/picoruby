require 'socket'

HOST = '0.0.0.0'
PORT = 5000

sock = UDPSocket.new
sock.connect(HOST, PORT)

sock.write("Hello, UDP server!")

begin
  sock.recv(1024) do |data|
    puts "Received: #{data}"
  end
rescue => e
  puts "No reply: #{e}"
end

sock.close
