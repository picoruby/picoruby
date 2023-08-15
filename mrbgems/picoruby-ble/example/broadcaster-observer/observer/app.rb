require 'ble'

class DemoObserver < BLE::Central
  def initialize
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
    super
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
    if 0 < found_devices.size
      suspend_scan
      puts "Found device(s) including 'PicoRuby' in name"
      print_found_devices
      clear_found_devices
      sleep 1
      @state = :TC_W4_SCAN_RESULT
      resume_scan
    end
  end
end

observer = DemoObserver.new
observer.debug = true
observer.scan(filter_name: "PicoRuby", stop_state: :no_stop)

