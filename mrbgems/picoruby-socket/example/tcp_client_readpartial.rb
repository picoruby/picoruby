require 'socket'

HOST = ARGV[0] || '127.0.0.1'
PORT = 8080

# Demonstrates readpartial raising EOFError when the server closes the connection.
#
# Run tcp_server_simple.rb first, then run this client.
# The server sends "ACK" and closes; the client reads until EOFError.

sock = TCPSocket.new(HOST, PORT)
puts "Connected to #{HOST}:#{PORT}"

sock.send("Hello!", 0)
puts "Sent: \"Hello!\""

response = ""
begin
  while true
    chunk = sock.readpartial(4)  # small maxlen to show multiple chunks
    response << chunk
    puts "  chunk: #{chunk.inspect}"
  end
rescue EOFError
  puts "  EOF reached"
end

puts "Response: #{response.inspect}"
sock.close
