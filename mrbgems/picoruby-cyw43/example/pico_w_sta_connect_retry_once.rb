require "cyw43"
require "machine"

# Minimal STA example to check connect_timeout behavior on Pico 2 W.

module PicoStaConnectRetryOnce
  # Replace these locally before running the example.
  # Do not commit real Wi-Fi credentials.
  STA_SSID = "YOUR_WIFI_SSID"
  STA_PASSWORD = "YOUR_WIFI_PASSWORD"
  CONNECT_TIMEOUT_SECONDS = 30
  RETRY_DELAY_MS = 100

  class << self
    def run
      puts "STA connect retry-once example"
      puts "SSID: #{STA_SSID}"
      puts "Password length: #{STA_PASSWORD.length}"

      CYW43.init("JP")
      CYW43.enable_sta_mode

      puts "connect_timeout attempt=1"
      result = connect_once
      puts "connect_timeout attempt=1 result=#{result.inspect}"

      if result == -8
        puts "connect_timeout attempt=1 retrying once after -8"
        sleep_ms RETRY_DELAY_MS

        puts "connect_timeout attempt=2"
        result = connect_once
        puts "connect_timeout attempt=2 result=#{result.inspect}"
      end

      unless result == true
        puts "connect_timeout failed: #{result.inspect}"
        return false
      end

      puts "STA connected"
      puts "STA IP: #{CYW43.ipv4_address || 'unassigned'}"
      puts "STA example ready"

      loop do
        sleep_ms 1000
      end
    rescue => e
      puts "STA boot error: #{e.class}: #{e.message}"
      false
    end

    def connect_once
      CYW43.connect_timeout(STA_SSID, STA_PASSWORD, CYW43::Auth::WPA2_MIXED_PSK, CONNECT_TIMEOUT_SECONDS)
    rescue => e
      puts "connect_timeout exception=#{e.class}: #{e.message}"
      retryable_error_code(e) || false
    end

    def retryable_error_code(exception)
      return -8 if exception.message.include?("-8")
      nil
    end
  end
end

PicoStaConnectRetryOnce.run
