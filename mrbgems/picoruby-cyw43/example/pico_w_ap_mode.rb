require "cyw43"

# Minimal AP mode example for Pico W / Pico 2 W.

CYW43.init("JP")
unless CYW43.enable_ap_mode("PicoRuby-AP", "12345678")
  raise "failed to enable AP mode"
end

puts "AP started"
puts "AP active?: #{CYW43.ap_active?}"
puts "AP SSID: #{CYW43.ap_ssid || 'unknown'}"
puts "AP IP: #{CYW43.ap_ipv4_address || 'unassigned'}"
puts "AP netmask: #{CYW43.ap_ipv4_netmask || 'unassigned'}"

led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)

loop do
  led.write(1)
  sleep 0.5
  led.write(0)
  sleep 0.5
end
