module Kernel
  alias :require_bak :require
  def require(gem)
    begin
      require_bak(gem)
    rescue LoadError
    end
  end
end

require_relative "../mrblib/ble.rb"
require_relative "../mrblib/ble_utils.rb"
require_relative "../mrblib/ble_gatt_database.rb"

class MbedTLS
  class CMAC
    def initialize(key, digest)
    end
    def update(data)
    end
    def digest
      "0123456789abcdef"
    end
  end
end

READ = BLE::READ
WRITE_WITHOUT_RESPONSE = BLE::WRITE_WITHOUT_RESPONSE
WRITE = BLE::WRITE
NOTIFY = BLE::NOTIFY
INDICATE = BLE::INDICATE
DYNAMIC = BLE::DYNAMIC

SERVICE_ENVIRONMENTAL_SENSING = 0x181A
CHARACTERISTIC_TEMPERATURE = 0x2A6E

db = BLE::GattDatabase.new do |db|
  db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, BLE::GAP_SERVICE_UUID) do |s|
    s.add_characteristic(BLE::GAP_DEVICE_NAME_UUID, READ, "PicoRuby BLE")
  end
  db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, BLE::GATT_SERVICE_UUID) do |s|
    s.add_characteristic(BLE::CHARACTERISTIC_DATABASE_HASH, READ)
  end
  db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, SERVICE_ENVIRONMENTAL_SENSING) do |s|
    s.add_characteristic(CHARACTERISTIC_TEMPERATURE, READ|NOTIFY|INDICATE|DYNAMIC)
  end
end

i = 0
len = nil
puts db.profile_data[0].ord.to_s(16).rjust(2, "0")
db.profile_data[1, db.profile_data.length - 1].each_byte do |b|
  i += 1
  len = b if i == 1
  len = (b << 8) + len if i == 2
  print b.to_s(16).rjust(2, "0"), " "
  if i == len
    puts
    i = 0
  end
end

puts

pp db.handle_table

# assume that the database hash will be calculated by MbedTLS::CMAC
# 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,
# 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a,
#
# 0x04, 0x00, 0x00, 0x28, 0x01, 0x18,
# 0x05, 0x00, 0x03, 0x28, 0x02, 0x06, 0x00, 0x2a, 0x2b,
#
# 0x07, 0x00, 0x00, 0x28, 0x1a, 0x18,
# 0x08, 0x00, 0x03, 0x28, 0x32,
# 0x09, 0x00, 0x6e, 0x2a,
# 0x0a, 0x00, 0x02, 0x29,
#
# db_hash = [0xd9, 0x9e, 0xb6, 0x01, 0xab, 0xc5, 0xab, 0x97, 0xcf, 0x26, 0x35, 0x4a, 0xbb, 0x4b, 0xc5, 0xef]
