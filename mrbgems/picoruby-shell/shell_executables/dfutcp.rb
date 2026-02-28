require 'dfu'

begin
  require 'socket'
rescue LoadError
  puts "dfutcp: socket library not available"
  exit 1
end

port = (ARGV[0] || 4649).to_i
path = ARGV[1]  # optional: destination path (skips A/B slot and meta)

puts "DFU TCP server listening on port #{port}"

server = TCPServer.new("0.0.0.0", port)

conn = server.accept
puts "DFU: connection accepted"
reboot_required = false
begin
  updater = DFU::Updater.new(path: path)
  updater.receive(conn)
  if path
    conn.write("OK\n")
    puts "DFU: file saved to #{path}"
  else
    DFU.confirm
    conn.write("OK\n")
    puts "DFU: update complete."
    reboot_required = true
  end
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
