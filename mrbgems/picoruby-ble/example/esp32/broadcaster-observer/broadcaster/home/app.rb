require 'ble'

class DemoBroadcaster < BLE
  def initialize
    super(:broadcaster, nil)
    @counter = 0
  end

  def adv_data(send_data)
    BLE::AdvertisingData.build do |adv|
      adv.add(0x01, 0xFF) # BLUETOOTH_DATA_TYPE_FLAGS
      adv.add(0x09, "PicoRuby") # complete local name
      adv.add(0xFF, send_data) # manufacturer specific data
    end
  end

  # No thermo/LCD wiring assumed on CoreS3 for this smoke test — a fake
  # sawtooth value stands in so an Observer can confirm the payload changes.
  def heartbeat_callback
    @counter += 1
    fake_temp = 2000 + (@counter % 100) * 10
    advertise(adv_data(fake_temp.to_s))
    puts "[broadcaster] advertising fake_temp=#{fake_temp}"
  end

  def packet_callback(event_packet)
    return unless event_packet[0]&.ord == BLE::BTSTACK_EVENT_STATE
    return unless event_packet[2]&.ord == BLE::HCI_STATE_WORKING
    puts "[broadcaster] up on `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
  end
end

broadcaster = DemoBroadcaster.new
broadcaster.debug = true
broadcaster.start
