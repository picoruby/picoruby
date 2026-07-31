require 'ble'

class DemoObserver < BLE
  TARGET_NAME = "PicoRuby"

  def initialize
    # Scan-only is the observer role, but mrblib gates scan and
    # advertising_report_callback on :central, so that is what the stack is
    # initialized as. This demo simply never calls connect. The port itself
    # treats BLE_ROLE_OBSERVER and BLE_ROLE_CENTRAL identically on the
    # advertising-report path (ports/esp32/ble.c:482).
    super(:central)
  end

  def advertising_report_callback(adv_report)
    return unless adv_report.name_include?(TARGET_NAME)
    fake_temp = adv_report.reports[:manufacturer_specific_data]
    puts "[observer] saw `#{TARGET_NAME}` rssi=#{adv_report.rssi} fake_temp=#{fake_temp}"
  end

  def heartbeat_callback
    puts "[observer] scanning..."
  end
end

observer = DemoObserver.new
observer.debug = true
observer.scan(stop_state: :no_stop)
