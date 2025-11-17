require 'socket'

HOST = '127.0.0.1'
PORT = 8080

begin
  sock = TCPSocket.new(HOST, PORT)
  puts "Connected to server at #{HOST}:#{PORT}"

  message = "Hello, TCP server!"
  sock.send(message)
  puts "Sent: #{message.inspect}"

  response = sock.recv(1024)
  puts "Received: #{response.inspect}"

  sock.close
  puts "Connection closed"
rescue => e
  puts "Error: #{e.message}"
end
