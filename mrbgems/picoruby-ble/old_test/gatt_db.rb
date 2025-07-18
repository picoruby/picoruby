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

module MbedTLS
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

SERVICE_ENVIRONMENTAL_SENSING = 0x181A
CHARACTERISTIC_TEMPERATURE = 0x2A6E
GAP_DEVICE_NAME =            0x2a00

SERVICE_GENERIC_ATTRIBUTE = 0x1801
SERVICE_ENVIRONMENTAL_SENSING = 0x181A

GATT_CHARACTERISTIC_USER_DESCRIPTION   =     0x2901

    db = BLE::GattDatabase.new do |db|
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, BLE::GAP_SERVICE_UUID) do |s|
        s.add_characteristic(BLE::READ, BLE::GAP_DEVICE_NAME_UUID, BLE::READ, "picow_temp")
      end
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, BLE::GATT_SERVICE_UUID) do |s|
        database_hash_key = 0.chr * 16
        s.add_characteristic(BLE::READ, BLE::CHARACTERISTIC_DATABASE_HASH, BLE::READ, database_hash_key) do |c|
          c.add_descriptor(BLE::READ, GATT_CHARACTERISTIC_USER_DESCRIPTION, "Database Hash")
        end
      end
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, SERVICE_ENVIRONMENTAL_SENSING) do |s|
        s.add_characteristic(BLE::READ|BLE::NOTIFY|BLE::INDICATE|BLE::DYNAMIC, CHARACTERISTIC_TEMPERATURE, BLE::READ|BLE::DYNAMIC, "") do |c|
          c.add_descriptor(BLE::READ|BLE::WRITE|BLE::WRITE_WITHOUT_RESPONSE|BLE::DYNAMIC, BLE::CLIENT_CHARACTERISTIC_CONFIGURATION, "\x00\x00")
        end
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

pp db.handle_table[SERVICE_ENVIRONMENTAL_SENSING][CHARACTERISTIC_TEMPERATURE][:value_handle]
pp db.handle_table[SERVICE_ENVIRONMENTAL_SENSING][CHARACTERISTIC_TEMPERATURE][BLE::CLIENT_CHARACTERISTIC_CONFIGURATION]


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
