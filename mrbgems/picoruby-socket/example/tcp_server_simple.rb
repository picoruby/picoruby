require 'socket'

server = TCPServer.new(nil, 8080)
puts "Simple TCP server listening on port 8080"

loop do
  client = server.accept
  puts "Client connected"

  data = client.recv(1024)
  puts "Received: #{data.inspect}"

  client.send("ACK", 0)
  client.close
  puts "Client disconnected"
end
