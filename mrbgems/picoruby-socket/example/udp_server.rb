require 'socket'

host = ARGV[0] || '0.0.0.0'
port = 5000

sock = UDPSocket.new
sock.bind(host, port)
puts "UDP server started on #{host}:#{port}"

loop do
  message, sender = sock.recvfrom(1024)
  puts "from #{sender[3]}:#{sender[1]} => #{message.inspect}"
  sock.send("OK", 0, sender[3], sender[1])
end
