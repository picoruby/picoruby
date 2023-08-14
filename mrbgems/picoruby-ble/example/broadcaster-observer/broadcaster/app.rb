require 'adc'
require 'ble'

class DemoBroadcaster < BLE::Broadcaster
  # for advertising
  APP_AD_FLAGS = 0x06
  BLUETOOTH_DATA_TYPE_FLAGS = 0x01
  BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS = 0x03
  BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09

  def initialize
    super(nil)
    led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @adc = ADC.new(:temperature)
  end

  def adv_data
    temperature = ((27 - (@adc.read * 3.3 / (1<<12) - 0.706) / 0.001721) * 100).to_i
    BLE::AdvertisingData.build do |a|
      a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, "PicoRuby BLE")
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, SERVICE_ENVIRONMENTAL_SENSING)
    end
  end

  def heartbeat_callback
    # do nothing
  end

  def packet_callback(event_packet)
    case event_packet[0]&.ord # event type
    when BTSTACK_EVENT_STATE
      return unless event_packet[2]&.ord ==  BLE::HCI_STATE_WORKING
      debug_puts "Broadcaster is up and running on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
      advertise(adv_data)
    end
  end
end

broadcaster = DemoBroadcaster.new
broadcaster.debug = true
broadcaster.start
