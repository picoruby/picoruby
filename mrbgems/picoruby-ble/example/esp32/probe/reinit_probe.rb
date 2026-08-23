require 'ble'

# Regression probe for BLE._init leaks (event queue, value hashes, GATT profile copy); N alternating-role cycles substitute for heap measurement.
N = 20

class ReinitPeripheral < BLE
  def initialize
    db = GattDatabase.new do |d|
      d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
        s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, "reinit_probe")
      end
    end
    super(:peripheral, db.profile_data)
  end

  def packet_callback(event_packet)
  end
end

class ReinitCentral < BLE
  def initialize
    super(:central)
  end
end

N.times do |i|
  ReinitPeripheral.new.start(300, :no_stop)
  ReinitCentral.new.start(300, :no_stop)
  puts "[probe] cycle=#{i} ok"
end

puts "[probe] SUMMARY completed=#{N} cycles without crash"
