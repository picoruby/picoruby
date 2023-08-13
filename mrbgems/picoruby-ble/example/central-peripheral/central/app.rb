require 'ble'

class DemoCentral < BLE::Central
  def initialize
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
    super
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
  end
end

$central = DemoCentral.new
$central.debug = true
$central.scan("PicoRuby")
if $central.found_devices.count == 1
  puts "Found device including 'PicoRuby' in name"
  $central.connect 0
  puts "Run irb and type '$central'"
else
  puts "No device found"
end
