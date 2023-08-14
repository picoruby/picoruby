require 'adc'
require 'ble'

class DemoBroadcaster < BLE::Broadcaster

  def initialize
    super(nil)
    led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @adc = ADC.new(:temperature)
  end

  def adv_data
    temperature = ((27 - (@adc.read * 3.3 / (1<<12) - 0.706) / 0.001721) * 100).to_i
    BLE::AdvertisingData.build do |a|
      # bluetooth_data_type_manufacturer_specific_data = 0xFF
      a.add(0xFF, 0xFF, 0xFF, 1,2,3,4,5,6,7)
      # bluetooth_data_type_complete_local_name = 0x09
      a.add(0x09, "PicoRuby BLE")
    end
  end

  def heartbeat_callback
    # do nothing
  end

  def packet_callback(event_packet)
    case event_packet[0]&.ord # event type
    when 0x60 # BTSTACK_EVENT_STATE
      return unless event_packet[2]&.ord ==  BLE::HCI_STATE_WORKING
      debug_puts "Broadcaster is up and running on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
      advertise(adv_data)
    end
  end
end

broadcaster = DemoBroadcaster.new
broadcaster.debug = true
broadcaster.start
