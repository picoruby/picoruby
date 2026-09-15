require 'ble'

class DemoCentral < BLE
  TARGET_NAME = "PicoRuby"

  def initialize
    super(:central)
  end

  def advertising_report_callback(adv_report)
    return unless adv_report.name_include?(TARGET_NAME)
    puts adv_report.format
    connect(adv_report)
  end
end

central = DemoCentral.new
central.scan(timeout_ms: 30_000, debug: true)

central.services.each do |service|
  puts sprintf("Service 0x%04X", service[:uuid32] || 0)
  service[:characteristics].each do |chara|
    puts sprintf("  Characteristic 0x%04X value: %s", chara[:uuid32] || 0, chara[:value].inspect)
  end
end
