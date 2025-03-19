require 'watchdog'
require 'ble'
require 'lcd'
require 'thermo'

class DemoBroadcaster < BLE

  def initialize
    super(:broadcaster, nil)
    led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @lcd = LCD.new(i2c: I2C.new(unit: :RP2040_I2C1, sda_pin: 26, scl_pin: 27))
    @thermo = THERMO.new(unit: :RP2040_SPI0, cipo_pin: 16, cs_pin: 17, sck_pin: 18, copi_pin: 19)
    @counter = 0
  end

  def adv_data(send_data)
    BLE::AdvertisingData.build do |adv|
      # BLUETOOTH_DATA_TYPE_FLAGS = 0x01
      adv.add(0x01, 0xFF)
      # bluetooth_data_type_complete_local_name = 0x09
      adv.add(0x09, "PicoRuby")
      # BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA = 0xFF
      adv.add(0xFF, send_data)
    end
  end

  def heartbeat_callback
    Machine.using_delay do
      temperature = @thermo.read
      @lcd.clear
      @lcd.print sprintf("%5.2f \xdfC", temperature)
      @lcd.break_line
      @lcd.print "+" * (@counter % 9)
      advertise(adv_data (temperature * 100).to_i.to_s)
    end
    @counter += 1
    Watchdog.update
  end

  def packet_callback(event_packet)
    case event_packet[0]&.ord # event type
    when 0x60 # BTSTACK_EVENT_STATE
      return unless event_packet[2]&.ord ==  BLE::HCI_STATE_WORKING
      puts "Broadcaster is up and running on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
      @state = :HCI_STATE_WORKING
      Watchdog.enable(2000)
    end
  end
end

broadcaster = DemoBroadcaster.new
broadcaster.start
