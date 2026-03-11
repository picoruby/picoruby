require 'net/ntp'

ntp_host = ARGV[0] || "pool.ntp.org"
ntp_port = 123

puts "Initializing DNS..."
retry_count = 0
# @type const MAX_DNS_RETRIES: Integer
MAX_DNS_RETRIES = 10
dns_ready = false
while retry_count < MAX_DNS_RETRIES
  begin
    test_socket = UDPSocket.new
    test_socket.connect(ntp_host, ntp_port)
    test_socket.close
    dns_ready = true
    puts "DNS ready"
    break
  rescue => e
    puts "DNS check failed: #{e.class} - #{e.message}"
    retry_count += 1
    sleep_ms 500
  end
end

# @type const MAX_NTP_RETRIES: Integer
MAX_NTP_RETRIES = 3

if dns_ready
  puts "Getting time from #{ntp_host} ..."
  ts = nil
  retries = 0
  while retries < MAX_NTP_RETRIES
    retries += 1
    begin
      ts = Net::NTP.get(ntp_host, ntp_port, 5)
      break if ts
    rescue => e
      puts "Attempt #{retries}/#{MAX_NTP_RETRIES} failed: #{e.message}"
      sleep_ms 500
    end
  end
  if ts.is_a?(Integer)
    Machine.set_hwclock(ts)
    puts "Current time: #{Time.now}"
  else
    puts "NTP request failed after #{MAX_NTP_RETRIES} attempts"
  end
else
  puts "DNS initialization failed after #{MAX_DNS_RETRIES * 500}ms"
end
