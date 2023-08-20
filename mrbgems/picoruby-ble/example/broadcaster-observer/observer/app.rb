require 'ble'

class DemoObserver < BLE::Observer
  def initialize
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
    super
    @count = 0
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
    if device_found?
      puts @count
      print_found_devices
      clear_found_devices
      @state = :TC_W4_SCAN_RESULT
    end
    puts @count += 1
  end
end

observer = DemoObserver.new
observer.scan(
  scan_type: :passive,
  scan_interval: 300,
  scan_window: 30,
  filter_name: "PicoRuby",
  stop_state: :no_stop,
  debug: true
)

