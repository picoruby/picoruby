require 'net/ntp'

# Example using a specific NTP server with custom timeout

puts "Querying time.nist.gov NTP server..."

begin
  # Use NIST NTP server with 10 second timeout
  unix_time = Net::NTP.get("time.nist.gov", 123, 10)

  puts "Unix timestamp: #{unix_time}"

  # Calculate time in seconds since 2000-01-01
  # Unix time for 2000-01-01 00:00:00 is 946684800
  seconds_since_2000 = unix_time - 946684800
  puts "Seconds since 2000-01-01: #{seconds_since_2000}"

rescue => e
  puts "Error: #{e.message}"
end
