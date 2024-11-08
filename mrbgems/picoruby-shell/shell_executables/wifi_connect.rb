# wifi_connect.rb
#   To connect to a WiFi network, this script reads the configuration
#   file at the path specified by the environment variable WIFI_CONFIG_PATH.
#   The configuration file should be a YAML file with the following format:
#   ```
#   # /etc/network/wifi.yml
#   country_code: "US"
#   wifi:
#     ssid: "my_wifi"
#     encoded_password: "base64_encoded_encrypted_password"
#     auto_connect: true
#     retry_if_failed: true
#   ```

require 'env'
require 'yaml'
require "mbedtls"
require "base64"
require 'net'

decrypt_proc = Proc.new do |decoded_password|
  cipher = MbedTLS::Cipher.new("AES-256-CBC")
  cipher.decrypt
  key_len = cipher.key_len
  iv_len = cipher.iv_len
  unique_id = Machine.unique_id
  len = unique_id.length
  cipher.key = (unique_id * ((key_len / len + 1) * len))[0, key_len].to_s
  cipher.iv = (unique_id * ((iv_len / len + 1) * len))[0, iv_len].to_s
  ciphertext = decoded_password[16, decoded_password.length - 16]
  cipher.update(ciphertext) + cipher.finish
end

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
  encoded_password = config['wifi']['encoded_password']
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

if check_auto_connect && !config["wifi"]["auto_connect"]
  puts "Auto connect is disabled"
  return
end

puts "Setting up WiFi as a station"
CYW43.enable_sta_mode

decoded_password = Base64.decode64(encoded_password)
password = decrypt_proc.call(decoded_password)

if CYW43.connect_blocking(ssid, password, CYW43::Auth::WPA2_MIXED_PSK)
  puts "Connected to WiFi network: #{ssid}"
else
  puts "Failed to connect to WiFi network: #{ssid}"
end

Net::NTP.set_hwclock
puts "Time set to #{Time.now}"
