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
#
# Watchdog (watchdog: true)
#   The watchdog only catches a hung WiFi driver.
#   wifi_connect enables the watchdog before the first connection attempt.
#   It feeds the watchdog before each Network::WiFi.connect_timeout call.
#   It disables the watchdog when the connection step ends:
#     - The connection succeeds.
#     - wifi_connect aborts.
#     - An exception occurs.
#
#   The device reboots only when one attempt does not return in time:
#     RP2040/RP2350: watchdog 8388 ms, attempt timeout 7 s
#     ESP32:         watchdog 15000 ms, attempt timeout 10 s
#
#   The device does not reboot in these cases:
#     - The attempt fails or times out normally.
#       If retry_if_failed is true, wifi_connect retries with no limit.
#       Otherwise, it prints "Abort." and returns.
#     - The configuration is invalid, or Network::WiFi.init fails.
#     - The connection succeeds, but no IP address comes in 2 seconds.
#     - The link goes down after wifi_connect returns.
#     - An exception other than Network::ConnectTimeout occurs.
#       At boot, the exception stops r2p2, and app.rb does not start.
#
# Guideline for app.rb
#   At boot, r2p2 loads app.rb after wifi_connect returns.
#   This also occurs when the connection failed.
#   Thus, do not assume that the network is available in app.rb.
#   Check Network::WiFi.link_connected? before a network operation.
#   Reboot the device (Watchdog.reboot) only when a reboot can repair the error:
#     - A network call does not return (a driver hang).
#       Enable the watchdog before the call, and feed it in the main loop.
#     - The link went down, and the reconnection fails again and again.
#       Retry with a delay first. Reboot after a limit of retries.
#   A reboot does not repair a wrong password, a missing access point,
#   or an invalid configuration. A reboot loop only hides these errors.

require 'env'
require 'yaml'
require "mbedtls"
require "base64"
require 'network'

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

# The YAML file is expected to have the following shape:
#   { "wifi" => { "ssid" => String, "encoded_password" => String,
#                 "auto_connect" => bool, "retry_if_failed" => bool,
#                 "watchdog" => bool },
#     "country_code" => String }
# @type var config: Hash[String, untyped]
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

country_code = config['country_code']
unless country_code
  puts "Country code not found in configuration file"
  return
end
if Network::WiFi.initialized?
  puts "Network::WiFi already initialized. Skipping country code setting."
else
  puts "Setting country code to #{country_code}"
  unless Network::WiFi.init country_code
    puts "Failed to initialize Network::WiFi"
    return # raising an exception here may cause a crash
  end
end

if check_auto_connect && !config["wifi"]["auto_connect"]
  puts "Auto connect is disabled"
  return
end

puts "Setting up WiFi as a station"
Network::WiFi.enable_sta_mode

if encoded_password.nil? || encoded_password.empty?
  auth = Network::WiFi::Auth::OPEN
  password = nil
else
  auth = Network::WiFi::Auth::WPA2_MIXED_PSK
  decoded_password = Base64.decode64(encoded_password)
  password = decrypt_proc.call(decoded_password)
end

watchdog = config["wifi"]["watchdog"]
# connect_timeout blocks in C without feeding the watchdog,
# so its timeout must be shorter than the watchdog period.
# See "Watchdog" at the top of this file.
timeout = 10 # seconds
if RUBY_PLATFORM == "ESP32"
  watchdog_ms = 15000
else
  watchdog_ms = Watchdog::RP2040_MAX_ENABLE_MS # 8388 ms
  timeout = 7 if watchdog
end

if watchdog
  Watchdog.enable watchdog_ms
  puts "Watchdog enabled (#{watchdog_ms}ms)"
end

retry_if_failed = config["wifi"]["retry_if_failed"]
# Do not use `retry` here: mruby/c runs the ensure clause on every retry
begin
  puts "Connecting to WiFi network: #{ssid}. Timeout in #{timeout} seconds..."

  while true
    Watchdog.feed if watchdog
    begin
      connected = Network::WiFi.connect_timeout(ssid, password, auth, timeout)
      reason = "Failed to connect."
    rescue Network::ConnectTimeout
      connected = false
      reason = "Connection timed out."
    end
    break if connected

    if retry_if_failed
      puts "#{reason} Retrying..."
      sleep 1
    else
      puts "#{reason} Abort."
      return
    end
  end
  puts "Connected."
ensure
  if watchdog
    Watchdog.disable
    puts "Watchdog disabled"
  end
end

puts "Waiting for IP address..."
retry_count = 0
max_retries = 20
until Network::WiFi.link_connected?
  sleep_ms 100
  retry_count += 1
  if retry_count >= max_retries
    puts "Failed to get IP address after #{max_retries * 100}ms"
    return
  end
end
puts "IP address obtained (#{Network::WiFi.tcpip_link_status_name})"

ARGV.clear
load "/bin/ntpdate"
