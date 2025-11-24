require 'net/ntp'

# Basic NTP usage example
# Get current time from NTP server

puts "Querying NTP server..."

begin
  # Get time from default NTP server (pool.ntp.org)
  unix_time = Net::NTP.get

  # Convert Unix timestamp to human-readable format
  # Note: This example assumes Time class is available
  time = Time.at(unix_time)
  puts "Current time: #{time}"

rescue => e
  puts "Error: #{e.message}"
end
