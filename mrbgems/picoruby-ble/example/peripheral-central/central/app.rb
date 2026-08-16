require 'ble'

# ble.rb defines a no-op CYW43 on boards without the chip. Giving it a GPIO
# that prints keeps the demo below identical on Pico W and everywhere else.
unless CYW43.const_defined?(:GPIO)
  class CYW43
    class GPIO
      LED_PIN = 0
      def initialize(pin)
      end
      def write(value)
        puts "led: #{value}"
      end
    end
  end
end

class DemoCentral < BLE
  TARGET_NAME = "PicoRuby"

  def initialize
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
    super(:central)
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
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
