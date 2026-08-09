require 'ble'

# Counts inbound writes during a flood so the device side can be compared with
# what PicoFloodTest sent. Counterpart: ports/darwin/test/test_flood.swift.
#
# Prints every 50th arrival rather than every one: at 500 frames a per-frame
# print would itself stall the VM thread and change what is being measured.
class WriteFloodProbe < BLE
  ADV_FLAGS     = 0x06
  AD_TYPE_FLAGS = 0x01
  AD_TYPE_NAME  = 0x09

  EVT_DISCONNECT = 0x05
  EVT_STATE      = 0x60

  SERVICE     = 0x181A
  CHAR_NOTIFY = 0x2A6E
  CHAR_WRITE  = 0x2A9F

  def initialize
    @adv_data = AdvertisingData.build do |a|
      a.add(AD_TYPE_FLAGS, ADV_FLAGS)
      a.add(AD_TYPE_NAME, "PBLE-FLOOD")
    end
    db = GattDatabase.new do |d|
      d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
        s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, "esp32_flood")
      end
      d.add_service(GATT_PRIMARY_SERVICE_UUID, SERVICE) do |s|
        s.add_characteristic(READ|NOTIFY|DYNAMIC, CHAR_NOTIFY, READ|DYNAMIC, "") do |c|
          c.add_descriptor(READ|WRITE|WRITE_WITHOUT_RESPONSE|DYNAMIC,
                           CLIENT_CHARACTERISTIC_CONFIGURATION, "\x00\x00")
        end
        s.add_characteristic(READ|WRITE|WRITE_WITHOUT_RESPONSE|DYNAMIC,
                             CHAR_WRITE, READ|WRITE|DYNAMIC, "")
      end
    end
    @write_handle = db.handle_table[SERVICE][CHAR_WRITE][:value_handle]
    super(:peripheral, db.profile_data)
    @received = 0
    @bytes = 0
    puts "[flood-probe] write_handle=#{@write_handle}"
  end

  def packet_callback(event_packet)
    case event_packet.getbyte(0)
    when EVT_STATE
      return unless event_packet.getbyte(2) == HCI_STATE_WORKING
      puts "[flood-probe] up, advertising as PBLE-FLOOD"
      advertise(@adv_data)
    when EVT_DISCONNECT
      puts "[flood-probe] disconnected received=#{@received} bytes=#{@bytes}"
      advertise(@adv_data)
    end
  end

  def heartbeat_callback
    while (v = pop_write_value(@write_handle))
      @received += 1
      @bytes += v.bytesize
      puts "[flood-probe] received=#{@received} bytes=#{@bytes}" if @received % 50 == 0
    end
  end
end

probe = WriteFloodProbe.new
probe.start
