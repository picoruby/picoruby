require 'ble'

class DemoObserver < BLE
  TARGET_NAME = "PicoRuby"

  def initialize
    super(:observer)
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
