require 'net/ntp'

ntp_host = ARGV[0] || "pool.ntp.org"
ntp_port = 123

puts "Initializing DNS..."
retry_count = 0
max_dns_retries = 10
dns_ready = false
while retry_count < max_dns_retries
  begin
    test_socket = UDPSocket.new
    test_socket.connect(ntp_host, ntp_port)
    test_socket.close
    dns_ready = true
    puts "DNS ready"
    break
  rescue => e
    print "."
    retry_count += 1
    sleep_ms 500
  end
end

if dns_ready
  puts "Getting time from #{ntp_host} ..."
  ts = Net::NTP.get(ntp_host, ntp_port)
  Machine.set_hwclock(ts) if ts
  puts "Current time: #{Time.now}"
else
  puts "DNS initialization failed after #{max_dns_retries * 500}ms"
end
