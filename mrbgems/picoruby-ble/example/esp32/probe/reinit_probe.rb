require 'ble'

# Regression probe for the BLE._init memory-leak fixes upstream made across
# every mruby/mrubyc port: every BLE.new used to leak its event queue, both
# value hashes, and the GATT profile copy, and a peripheral's profile leaked
# specifically when a later BLE.new ran as central (role-changing re-init).
#
# Neither shows up as a functional difference from Ruby -- both are heap
# exhaustion over many cycles, not observable from a single instantiation.
# This probe substitutes repetition for measurement: N re-init cycles that
# alternate role (peripheral with a profile -> central with none) is the
# exact sequence the fix targets, run enough times that an unreleased
# profile or an unbounded GC pin count crashes or exhausts heap before
# reaching N, rather than reporting a byte count.
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
