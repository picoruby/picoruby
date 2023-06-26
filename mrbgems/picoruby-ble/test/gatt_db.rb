require_relative "/home/hasumi/work/r2p2/lib/picoruby/mrbgems/picoruby-ble/mrblib/ble.rb"

READ = BLE::ATT_PROPERTY_READ
WRITE_WITHOUT_RESPONSE = BLE::ATT_PROPERTY_WRITE_WITHOUT_RESPONSE
WRITE = BLE::ATT_PROPERTY_WRITE
NOTIFY = BLE::ATT_PROPERTY_NOTIFY
INDICATE = BLE::ATT_PROPERTY_INDICATE
DYNAMIC = 0x100

GAP_SERVICE_UUID = 0x1800
SERVICE_GENERIC_ATTRIBUTE = 0x1801
CHARACTERISTIC_DATABASE_HASH = 0x2B2A
SERVICE_ENVIRONMENTAL_SENSING = 0x181A
CHARACTERISTIC_TEMPERATURE = 0x2A6E
CHARACTERISTIC_GAP_DEVICE_NAME = 0x2A00

db = BLE::GattDatabase.build do |db|
  db.add_service(GAP_SERVICE_UUID) do |s|
    s.add_characteristic(CHARACTERISTIC_GAP_DEVICE_NAME, READ, 0x0002, "picow_temp")
  end
  db.add_service(SERVICE_GENERIC_ATTRIBUTE) do |s|
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
    db_hash = [0xd9, 0x9e, 0xb6, 0x01, 0xab, 0xc5, 0xab, 0x97, 0xcf, 0x26, 0x35, 0x4a, 0xbb, 0x4b, 0xc5, 0xef]
    s.add_characteristic(CHARACTERISTIC_DATABASE_HASH, READ, 0x0002, *db_hash)
  end
  db.add_service(SERVICE_ENVIRONMENTAL_SENSING) do |s|
    s.add_characteristic(CHARACTERISTIC_TEMPERATURE, READ|NOTIFY|INDICATE|DYNAMIC, 0x0102)
  end
end

i = 0
len = nil
db[1, db.length - 1].each_byte do |b|
  i += 1
  len = b if i == 1
  len = (b << 8) + len if i == 2
  print b.to_s(16).rjust(2, "0"), " "
  if i == len
    puts
    i = 0
  end
end
