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
  end

  def advertising_report_callback(adv_report)
    puts adv_report.format
    #if adv_report&.reports[:complete_local_name] == 'PicoRuby'
    #  now = Time.now.to_i
    #  if 2 < now - @last_time_found
    #    @uart.puts adv_report.format
    #    @last_time_found = now
    #  end
    #end
  end

  def heartbeat_callback
    @led.invert
  end
end

observer = DemoObserver.new
observer.scan(stop_state: :no_stop)

