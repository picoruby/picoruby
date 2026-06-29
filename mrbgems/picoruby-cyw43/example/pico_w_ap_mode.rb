require "cyw43"

# Minimal AP mode example for Pico W / Pico 2 W.
# TODO: add AP-side DHCP/IP helpers before expanding this into a supported HTTP sample.

CYW43.init("JP")
CYW43.enable_ap_mode("PICO-TIMER", "12345678")

puts "AP started"
puts "Clients may need a static IP until DHCP support is added."

led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)

loop do
  led.write(1)
  sleep 0.5
  led.write(0)
  sleep 0.5
end
