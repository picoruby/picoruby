require 'ble'

class DemoObserver < BLE::Observer
  def advertising_report_callback(adv_report)
    puts adv_report.format if adv_report.name_include?('PicoRuby')
  end
end

observer = DemoObserver.new
observer.scan(stop_state: :no_stop, debug: true)

