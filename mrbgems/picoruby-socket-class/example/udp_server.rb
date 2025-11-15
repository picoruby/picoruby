require 'socket'

HOST = '0.0.0.0'
PORT = 5000

sock = UDPSocket.new
sock.bind(HOST, PORT)

puts "UDP server started"

loop do
  message, sender = sock.recvfrom(1024)
  puts "from #{sender[3]}:#{sender[1]} => #{message.inspect}"
  sock.send("OK", 0, sender[3], sender[1])
end
