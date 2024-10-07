require 'yaml'
require 'cyw43'
require 'net'

unless File.exist?(ENV['WIFI_CONFIG_PATH'])
  puts "File #{ENV['WIFI_CONFIG_PATH']} does not exist"
  return
end

# Load the file

# @type var config: { "auto_connect": bool, "wifi": { "ssid": String, "password": String }, "country_code": String }

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

unless config["auto_connect"]
  puts "WiFi configuration file not found at #{ENV['WIFI_CONFIG_PATH']}"
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
