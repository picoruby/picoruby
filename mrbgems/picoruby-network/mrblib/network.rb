require "machine"

module Network
  case Machine.mcu_name
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
  when "POSIX"
    # no-op. Calling Network:WiFi raises error
  else
    # Shouold not reach here
  end
end
