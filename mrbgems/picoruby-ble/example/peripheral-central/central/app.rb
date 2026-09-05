require 'ble'

class DemoCentral < BLE
  def initialize
    super(:central)
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
  end

  def advertising_report_callback(adv_report)
    return unless adv_report.name_include?("PicoRuby")
    puts adv_report.format
    connect(adv_report)
  end
end

central = DemoCentral.new
central.scan(debug: true)
central.services.each do |service|
  puts sprintf("Service 0x%04X", service[:uuid32] || 0)
  service[:characteristics].each do |chara|
    puts sprintf("  Characteristic 0x%04X value: %s", chara[:uuid32] || 0, chara[:value].inspect)
  end
end
