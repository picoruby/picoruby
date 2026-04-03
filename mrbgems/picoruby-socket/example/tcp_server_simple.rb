require 'socket'

server = TCPServer.new(nil, 8080)
puts "Simple TCP server listening on port 8080"

while true
  client = server.accept
  puts "Client connected"

  begin
    data = client.readpartial(1024)
    puts "Received: #{data.inspect}"
    client.send("ACK", 0)
  rescue EOFError
    puts "Client disconnected before sending data"
  end

  client.close
  puts "Client disconnected"
end
