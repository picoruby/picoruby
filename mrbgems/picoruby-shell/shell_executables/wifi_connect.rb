require 'yaml'
require 'net'

check_auto_connect = false

ARGV.each do |arg|
  if arg == "--check-auto-connect"
    check_auto_connect = true
  else
    puts "Invalid argument: #{arg}"
    return
  end
end

unless File.exist?(ENV['WIFI_CONFIG_PATH'])
  puts "File #{ENV['WIFI_CONFIG_PATH']} does not exist"
  return
end

# Load the file

# @rbs config: { "auto_connect": bool, "wifi": { "ssid": String, "password": String }, "country_code": String }
config = File.open(ENV['WIFI_CONFIG_PATH'], "r") do |f|
  YAML.load(f.read.to_s)
end

begin
  ssid = config['wifi']['ssid']
  password = config['wifi']['password']
rescue
  puts "Invalid configuration file"
  return
end

country_code = config['country_code']
unless country_code
  puts "Country code not found in configuration file"
  return
end
puts "Setting country code to #{country_code}"
CYW43.init country_code

if check_auto_connect && !config["auto_connect"]
  puts "Auto connect is disabled"
  return
end

puts "Setting up WiFi as a station"
CYW43.enable_sta_mode

if CYW43.connect_blocking(ssid, password, CYW43::Auth::WPA2_MIXED_PSK)
  puts "Connected to WiFi network: #{ssid}"
else
  puts "Failed to connect to WiFi network: #{ssid}"
end

Net::NTP.set_hwclock
puts "Time set to #{Time.now}"
