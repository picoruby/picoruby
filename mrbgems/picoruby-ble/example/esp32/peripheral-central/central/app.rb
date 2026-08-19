require 'ble'

class DemoCentral < BLE
  TARGET_NAME = "PicoRuby"

  def initialize
    super(:central)
  end

  def advertising_report_callback(adv_report)
    return unless adv_report.name_include?(TARGET_NAME)
    puts "Found `#{TARGET_NAME}` rssi: #{adv_report.rssi}. Connecting..."
    connect(adv_report)
  end
end

central = DemoCentral.new
central.debug = true
central.scan(timeout_ms: 30_000)

if central.services.empty?
  puts "No service discovered"
else
  central.services.each do |service|
    puts "service uuid32: 0x#{service[:uuid32]&.to_s(16)}"
    service[:characteristics].each do |characteristic|
      puts "  characteristic uuid32: 0x#{characteristic[:uuid32]&.to_s(16)} value: #{characteristic[:value].inspect}"
    end
  end
end
