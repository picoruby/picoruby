# Pico
#led = GPIO.new(25, GPIO::OUT)

# Pico W
require "cyw43"
CYW43.init
led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)

puts "Hello World"
led.write 1
sleep 2
led.write 0

# Once Pico (W) enter low power mode,
# USB-CDC will not be restored.

while true
  led.write 1
  Machine.sleep(2)
  led.write 0
  Machine.sleep(2)
end
