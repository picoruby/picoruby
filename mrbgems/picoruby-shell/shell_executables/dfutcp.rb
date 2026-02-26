require 'dfu'

begin
  require 'socket'
rescue LoadError
  puts "dfutcp: socket library not available"
  exit 1
end

port = (ARGV[0] || 4649).to_i

puts "DFU TCP server listening on port #{port}"

server = TCPServer.new("0.0.0.0", port)

conn = server.accept
puts "DFU: connection accepted"
reboot_required = false
begin
  updater = DFU::Updater.new
  updater.receive(conn)
  DFU.confirm
  conn.write("OK\n")
  puts "DFU: update complete."
  reboot_required = true
rescue => e
  conn.write("ERROR: #{e.message}\n")
  puts "DFU: error - #{e.message}"
ensure
  conn.close
  server.close
end
if reboot_required
  Machine.reboot
end
