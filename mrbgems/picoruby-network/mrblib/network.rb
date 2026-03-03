require "machine"

module Network
  case Machine.mcu_name
  when "RP2040", "RP2350"
    begin
      require "cyw43"
      WiFi = CYW43
    rescue LoadError # Only LoadError should be rescued
    end
  when "ESP32"
    require "esp32"
    WiFi = ESP32::WiFi
  when "POSIX"
    # no-op. Calling Network:WiFi raises error
  else
    # Shouold not reach here
  end
end
