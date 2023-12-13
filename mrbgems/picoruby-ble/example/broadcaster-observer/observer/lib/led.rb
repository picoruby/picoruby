require 'gpio'
require 'cyw43'
CYW43.init

class LED
  def initialize
    @gpio = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    #     = GPIO.new(25, GPIO::OUT) # If Pico w/o W
  end

  def on
    @gpio.write 1
  end

  def off
    @gpio.write 0
  end

  def invert
    @gpio.read == 1 ? off : on
  end
end
