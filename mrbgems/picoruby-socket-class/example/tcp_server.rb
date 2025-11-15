require 'socket'

HOST = nil
PORT = 8080

server = TCPServer.new(HOST, PORT)

puts "TCP server started on port #{PORT}"
puts "Waiting for connections..."

begin
  server.accept_loop do |client|
    puts "Client connected"

    begin
      data = client.recv(1024)
      if data && data.length > 0
        puts "Received: #{data.inspect}"
        client.send("Echo: #{data}")
      end
    rescue => e
      puts "Error handling client: #{e.message}"
    ensure
      client.close
      puts "Client disconnected"
    end
  end
rescue Interrupt
  puts "\nShutting down server..."
ensure
  server.close
  puts "Server closed"
end
