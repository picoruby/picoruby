require 'ble'

# Verifies the ESP32 port's dynamic-read mirror: the VM thread pushes a value
# with push_read_value, the NimBLE host task must serve it from plain memory.
# Counterpart: ports/darwin/test/esp32_dynamic_read_test.rb.
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
        s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, "esp32_rdprobe")
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

  # Rewrites the value every tick so a mirror that copied once at init shows up
  # as a mismatch rather than passing. The Mac side rejects "STATIC" (the
  # profile's static value, what an unpopulated mirror returns) and requires
  # the RDPROBE- prefix.
  def heartbeat_callback
    @tick += 1
    value = "RDPROBE-#{@tick}"
    push_read_value(@read_handle, value)
    puts "[rdprobe] pushed #{value.inspect}" if @tick % 10 == 1
  end
end

probe = DynamicReadProbe.new
probe.start
