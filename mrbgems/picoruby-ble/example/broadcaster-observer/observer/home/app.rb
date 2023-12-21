require 'watchdog'
require 'ble'
require 'uart'
require 'led'

class DemoObserver < BLE::Observer
  def initialize
    super
    @led = LED.new
    @uart = UART.new(unit: :RP2040_UART0, txd_pin: 16, rxd_pin: 17, baudrate: 115200)
    @uart.puts 'Start BLE Observer Demo'
    @last_time_found = Time.now.to_i
    Watchdog.enable(4000)
  end

  def advertising_report_callback(adv_report)
    if adv_report&.reports[:complete_local_name] == 'PicoRuby'
      now = Time.now.to_i
      if 2 < now - @last_time_found
        @uart.puts sprintf("%5.2f degC", adv_report.reports[:manufacturer_specific_data].to_f / 100)
        @last_time_found = now
      end
    end
  end

  def heartbeat_callback
    Watchdog.update
    @led.invert
  end
end

observer = DemoObserver.new
observer.scan(stop_state: :no_stop)

