require 'ble'

# Drives and witnesses every peripheral-side path of the ESP32 NimBLE port.
# Counterpart: PicoCentralTest.app (ports/darwin/test/test_central.swift).
#
#   P1 MTU exchange (0xB5)        P5 WRITE_CHR seen via pop_write_value
#   P2 SUBSCRIBE / WRITE_DSC      P6 DISCONNECT (0x05) then rearm_adv
#   P3 CAN_SEND_NOW (0xB7)        P7 stop_advertise, reached by advertise(nil)
#   P4 notify                        (src/mruby/ble_peripheral.c:10-11)
#
# packet_callback receives the raw synthesized packet before anything parses
# it, so the port's own output is observable from Ruby.
class PeripheralPathsProbe < BLE
  ADV_FLAGS     = 0x06
  AD_TYPE_FLAGS = 0x01
  AD_TYPE_NAME  = 0x09

  EVT_DISCONNECT   = 0x05
  EVT_STATE        = 0x60
  EVT_MTU          = 0xB5
  EVT_CAN_SEND_NOW = 0xB7

  SERVICE     = 0x181A
  CHAR_NOTIFY = 0x2A6E
  CHAR_WRITE  = 0x2A9F

  def initialize
    @adv_data = AdvertisingData.build do |a|
      a.add(AD_TYPE_FLAGS, ADV_FLAGS)
      a.add(AD_TYPE_NAME, "PBLE-PROBE")
    end
    db = GattDatabase.new do |d|
      d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
        s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, "esp32_probe")
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
    @notify_handle = db.handle_table[SERVICE][CHAR_NOTIFY][:value_handle]
    @cccd_handle   = db.handle_table[SERVICE][CHAR_NOTIFY][CLIENT_CHARACTERISTIC_CONFIGURATION]
    @write_handle  = db.handle_table[SERVICE][CHAR_WRITE][:value_handle]
    super(:peripheral, db.profile_data)
    @counter = 0
    @notify_on = false
    @stop_requested = false
  end

  def packet_callback(event_packet)
    case event_packet.getbyte(0)
    when EVT_STATE
      return unless event_packet.getbyte(2) == HCI_STATE_WORKING
      puts "[probe] P0 up on #{Utils.bd_addr_to_str(gap_local_bd_addr)}"
      advertise(@adv_data)
    when EVT_MTU
      puts "[probe] P1 MTU_EXCHANGE_COMPLETE raw=#{event_packet.bytesize}"
    when EVT_CAN_SEND_NOW
      puts "[probe] P3 CAN_SEND_NOW"
      notify @notify_handle
      puts "[probe] P4 notify sent counter=#{@counter}"
    when EVT_DISCONNECT
      puts "[probe] P6 DISCONNECTION_COMPLETE"
      @notify_on = false
      if @stop_requested
        advertise(nil)
        puts "[probe] P7 stop_advertise called"
      else
        advertise(@adv_data)
        puts "[probe] P6 rearm: re-advertising"
      end
    end
  end

  def heartbeat_callback
    @counter += 1
    push_read_value(@notify_handle, Utils.int16_to_little_endian(@counter))

    if (v = pop_write_value(@cccd_handle))
      @notify_on = (v == "\x01\x00")
      puts "[probe] P2 CCCD written value=#{v.inspect} notify_on=#{@notify_on}"
    end
    if (v = pop_write_value(@write_handle))
      puts "[probe] P5 WRITE_CHR received value=#{v.inspect}"
      @stop_requested = true if v.include?("STOPADV")
    end
    request_can_send_now_event if @notify_on && @counter % 3 == 0
  end
end

probe = PeripheralPathsProbe.new
probe.start
