require "machine"

module Network
  case RUBY_PLATFORM
  when "RP2040", "RP2350"
    begin
      require "cyw43"
      WiFi = CYW43
      ConnectTimeout = CYW43::ConnectTimeout
    rescue LoadError # Only LoadError should be rescued
    end
  when "ESP32"
    begin
      require "esp32"
      WiFi = ESP32::WiFi
      ConnectTimeout = ESP32::ConnectTimeout
    rescue LoadError # Only LoadError should be rescued
    end
  when "NRF52"
    begin
      require "openthread"
      # Thread, not Wi-Fi: the constant sits beside WiFi rather than
      # implementing it. None of WiFi's contract applies -- there is no
      # SSID, no PSK, no IPv4 and no DHCP on a Thread mesh -- so a
      # program has to ask for the link it actually has.
      Thread = OpenThread
      ConnectTimeout = OpenThread::ConnectTimeout
    rescue LoadError # Only LoadError should be rescued
    end
  when "POSIX"
    # no-op. Calling Network:WiFi raises error
  else
    # Should not reach here
  end
end
