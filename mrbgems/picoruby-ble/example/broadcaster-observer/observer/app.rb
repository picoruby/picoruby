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
      start_scan
    end
    puts @count += 1
    if @count % 10 == 0
      puts "stop_scan"
      stop_scan
    elsif @count % 10 == 1
      puts "start_scan"
      start_scan
    end
  end
end

observer = DemoObserver.new
observer.heartbeat_period_ms = 1000
observer.scan(
  scan_type: :passive,
  scan_interval: 100,
  scan_window: 50,
  filter_name: "PicoRuby",
  stop_state: :no_stop,
  debug: true
)

