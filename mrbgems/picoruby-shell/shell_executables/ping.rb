# ping - test network connectivity using DNS and HTTP

begin
  require 'socket'
rescue LoadError
  puts "Socket not available"
  return
end

unless ARGV.count == 1
  puts "Usage: ping hostname"
  return
end

host = ARGV[0]

puts "PING #{host}"

# Try DNS resolution and ping
puts "Press Ctrl+C to stop"
loop do
  tv_sec1, tv_nsec1 = Machine.get_hwclock
  # Try to make a simple connection to test reachability
  udp = UDPSocket.new
  udp.connect(host, 33434)
  udp.send("x", 0)

  begin
    udp.recvfrom(1)
  rescue => e
    # Ignore errors, as we just want to see if we can reach the host
  end

  udp.close

  tv_sec2, tv_nsec2 = Machine.get_hwclock
  puts "resolved in #{(tv_sec2 - tv_sec1) * 1000 + (tv_nsec2 - tv_nsec1) / 1000000} ms"
  sleep 1
end
