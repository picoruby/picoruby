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
#     watchdog: true
#   ```

require 'env'
require 'yaml'
require "mbedtls"
require "base64"

wifi_module = ENV['WIFI_MODULE']
case wifi_module
when "cwy43"
  require 'cyw43'
when "esp32"
  require 'esp32'
else
  puts "No WiFi module available"
  return
end

decrypt_proc = Proc.new do |decoded_password|
  cipher = MbedTLS::Cipher.new("AES-256-CBC")
  cipher.decrypt
  key_len = cipher.key_len
  iv_len = cipher.iv_len
  unique_id = Machine.unique_id
  len = unique_id.length
  cipher.key = (unique_id * ((key_len / len + 1) * len))[0, key_len].to_s
  cipher.iv = (unique_id * ((iv_len / len + 1) * len))[0, iv_len].to_s
  # Based on successful debug run: CBC mode has empty tag, so entire data is ciphertext
  ciphertext = decoded_password
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

wifi_config_path = ENV['WIFI_CONFIG_PATH'] || ENV_DEFAULT_WIFI_CONFIG_PATH

unless File.file?(wifi_config_path)
  puts "File #{wifi_config_path} does not exist"
  return
end

# Load the file

# @rbs config: {
#        "wifi": {
#          "ssid": String,
#          "password": String,
#          "auto_connect": bool,
#          "retry_if_failed": bool
#        },
#        "country_code": String
#       }
config = File.open(wifi_config_path, "r") do |f|
  YAML.load(f.read.to_s)
end

begin
  ssid = config['wifi']['ssid']
  encoded_password = config['wifi']['encoded_password']
rescue
  puts "Invalid configuration file"
  return
end

case wifi_module
when "cwy43"
  country_code = config['country_code']
  unless country_code
    puts "Country code not found in configuration file"
    return
  end
  if CYW43.initialized?
    puts "CYW43 already initialized. Skipping country code setting."
  else
    puts "Setting country code to #{country_code}"
    unless CYW43.init country_code
      puts "Failed to initialize CYW43"
      return # raising an exception here may cause a crash
    end
  end
when "esp32"
  if ESP32::WiFi.initialized?
    puts "ESP32::WiFi already initialized."
  else
    unless ESP32::WiFi.init
      puts "Failed to initialize ESP32::WiFi"
      return
    end
  end
end

if check_auto_connect && !config["wifi"]["auto_connect"]
  puts "Auto connect is disabled"
  return
end

if encoded_password.nil? || encoded_password.empty?
  auth = case wifi_module
         when "cwy43" then CYW43::Auth::OPEN
         when "esp32" then ESP32::Auth::OPEN
         end
  password = nil
else
  auth = case wifi_module
         when "cwy43" then CYW43::Auth::WPA2_MIXED_PSK
         when "esp32" then ESP32::Auth::WPA2_MIXED_PSK
         end
  decoded_password = Base64.decode64(encoded_password)
  password = decrypt_proc.call(decoded_password)
end

case wifi_module
when "cwy43"
  puts "Setting up WiFi as a station"
  CYW43.enable_sta_mode
end

if config["wifi"]["watchdog"]
  Watchdog.enable 8388 # Max value is 8388ms
  puts "Watchdog enabled (8388ms)"
end

begin
  puts "Connecting to WiFi network: #{ssid}. Timeout in 10 seconds..."
  case wifi_module
  when "cwy43"
    CYW43.connect_timeout(ssid, password, auth, 10) # seconds
  when "esp32"
    ESP32::WiFi.connect_timeout(ssid, password, auth, 10) # seconds
  end
  puts "Connected."
rescue => e
  if config["wifi"]["retry_if_failed"]
    puts "Failed to connect. Retrying..."
    sleep 1
    retry
  else
    puts "Failed to connect. Abort."
    return
  end
end

if config["wifi"]["watchdog"]
  Watchdog.disable
  puts "Watchdog disabled"
end

link_up_check = case wifi_module
                when "cwy43" then Proc.new { CYW43.link_connected? }
                when "esp32" then Proc.new { ESP32::WiFi.link_connected? }
                end
link_status_name = case wifi_module
                   when "cwy43" then Proc.new { CYW43.tcpip_link_status_name }
                   when "esp32" then Proc.new { ESP32::WiFi.tcpip_link_status_name }
                   end

puts "Waiting for IP address..."
retry_count = 0
max_retries = 20
until link_up_check.call
  sleep_ms 100
  retry_count += 1
  if retry_count >= max_retries
    puts "Failed to get IP address after #{max_retries * 100}ms"
    return
  end
end
puts "IP address obtained (#{link_status_name.call})"

ARGV.clear
load "/bin/ntpdate"
