require 'machine'

# Blink, then sleep in the deepest timed mode -- the whole machine
# stops between blinks and wakes by the always-on timer. Execution
# continues after each Machine.sleep call; nothing reboots.
#
# On a plain Pico use GPIO 25 for the LED:
#   led = GPIO.new(25, GPIO::OUT)
# On a Pico W / Pico 2 W the LED hangs off the WiFi chip:
require 'cyw43'
CYW43.init
led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)

# DORMANT tears USB down and re-initializes it on wake: the host
# re-enumerates the device, so reopen the terminal to see output
# again. With deep: false the console stays alive, at more power.
#
# The timed-DORMANT minimum is 2000ms on RP2040, 10ms on RP2350.

i = 0
while i < 5
  led.write 1
  Machine.sleep(deep: true, source: :timer, ms: 2000)
  led.write 0
  Machine.sleep(deep: true, source: :timer, ms: 2000)
  i += 1
end
puts "5 blinks done"
