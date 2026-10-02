require 'ble'

# Checks that push_read_value (called from the VM thread) is actually served by the host stack thread, not a stale copy.
class DynamicReadProbe < BLE
  ADV_FLAGS     = 0x06
  AD_TYPE_FLAGS = 0x01
  AD_TYPE_NAME  = 0x09

  EVT_DISCONNECT = 0x05
  EVT_STATE      = 0x60

  SERVICE   = 0x181A
  CHAR_READ = 0x2A6E

  def initialize
    @adv_data = AdvertisingData.build do |a|
      a.add(AD_TYPE_FLAGS, ADV_FLAGS)
      a.add(AD_TYPE_NAME, "PBLE-RDPROBE")
    end
    db = GattDatabase.new do |d|
      d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
        s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, "picoruby_rdprobe")
      end
      d.add_service(GATT_PRIMARY_SERVICE_UUID, SERVICE) do |s|
        s.add_characteristic(READ|DYNAMIC, CHAR_READ, READ|DYNAMIC, "STATIC")
      end
    end
    @read_handle = db.handle_table[SERVICE][CHAR_READ][:value_handle]
    super(:peripheral, db.profile_data)
    @tick = 0
    puts "[rdprobe] read_handle=#{@read_handle}"
  end

  def packet_callback(event_packet)
    case event_packet.getbyte(0)
    when EVT_STATE
      return unless event_packet.getbyte(2) == HCI_STATE_WORKING
      puts "[rdprobe] up, advertising as PBLE-RDPROBE"
      advertise(@adv_data)
    when EVT_DISCONNECT
      puts "[rdprobe] disconnected at tick=#{@tick}"
      advertise(@adv_data)
    end
  end

  # New value every tick catches a stale mirror; peer requires the RDPROBE- prefix ("STATIC" means unpopulated).
  def heartbeat_callback
    @tick += 1
    value = "RDPROBE-#{@tick}"
    push_read_value(@read_handle, value)
    puts "[rdprobe] pushed #{value.inspect}" if @tick % 10 == 1
  end
end

probe = DynamicReadProbe.new
probe.start
