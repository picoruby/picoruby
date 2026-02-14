# PicoRuby WebSocket server example
# Run with: picoruby picoruby_server.rb

require 'net/websocket'

PORT = 8080
HOST = '0.0.0.0'

puts "Starting PicoRuby WebSocket server on ws://#{HOST}:#{PORT}"
puts "Press Ctrl+C to stop"

server = Net::WebSocket::Server.new(host: HOST, port: PORT)
server.start

puts "Server ready!"

server.accept_loop do |conn|
  puts "[Server] Client connected"

  begin
    loop do
      message = conn.receive(timeout: 60)

      if message.nil?
        puts "[Server] Timeout, closing connection"
        break
      end

      puts "[Server] Received: #{message.inspect}"

      # Echo server - send back what we received
      conn.send(message)
      puts "[Server] Echoed: #{message.inspect}"

      # Special commands for testing
      case message
      when "PING_TEST"
        conn.ping("ping_payload")
        puts "[Server] Sent ping with payload"
      when "CLOSE_TEST"
        conn.close(Net::WebSocket::CLOSE_NORMAL, "Normal closure")
        puts "[Server] Initiated close"
        break
      when "BINARY_TEST"
        binary_data = "\x00\x01\x02\x03\x04\x05"
        conn.send_binary(binary_data)
        puts "[Server] Sent binary test data"
      end
    end
  rescue Net::WebSocket::ConnectionClosed => e
    puts "[Server] Connection closed: #{e.message}"
  rescue => e
    puts "[Server] Error: #{e.class}: #{e.message}"
  ensure
    conn.close unless conn.closed?
    puts "[Server] Client disconnected"
  end
end
