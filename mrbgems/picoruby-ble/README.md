# picoruby-ble

Bluetooth Low Energy (BLE) library for PicoRuby - supports Central, Peripheral, Observer, and Broadcaster roles.

## Usage

### Peripheral (Server) Example

```ruby
require 'ble'

# Create GATT database
gatt_db = BLE::GattDatabase.new do |db|
  db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, 0x1234) do
    db.add_characteristic(
      BLE::READ | BLE::NOTIFY,
      0x5678,
      BLE::READ_ANYBODY,
      "Initial value"
    )
  end
end

# Initialize peripheral
ble = BLE.new(:peripheral, gatt_db.profile_data)

# Create advertising data
ad_data = BLE::AdvertisingData.build do |ad|
  ad.add(0x09, "MyDevice")  # Complete local name
end

# Start advertising
ble.peripheral_advertise(ad_data)

# Main loop
loop do
  ble.start(100)
  # Handle BLE events
end
```

### Central (Client) Example

```ruby
require 'ble'

ble = BLE.new(:central)

# Scan for devices
ble.scan(timeout_ms: 5000) do |report|
  puts "Found: #{report.format}"
  if report.name_include?("MyDevice")
    # Connect to device
    ble.connect(report)
    break
  end
end
```

### Observer (Scanning Only)

```ruby
ble = BLE.new(:observer)

ble.scan(scan_type: :passive, timeout_ms: 10000) do |report|
  puts "Device: #{report.format}"
  puts "RSSI: #{report.rssi}"
end
```

## API

### BLE Class

- `BLE.new(role, profile_data = nil)` - Initialize BLE
  - `role`: `:central`, `:peripheral`, `:observer`, or `:broadcaster`
  - `profile_data`: GATT database for peripheral role

- `start(timeout_ms = nil)` - Process BLE events
- `gap_local_bd_addr()` - Get local Bluetooth address
- `hci_power_control(power_mode)` - Control HCI power state

### Peripheral Methods

- `peripheral_advertise(ad_data)` - Start advertising
- `notify(handle)` - Send notification
- `pop_write_value(handle)` - Get value written by client
- `push_read_value(handle, value)` - Set value for read request

### Central Methods

- `scan(...)` - Scan for advertising devices
- `connect(advertising_report)` - Connect to device
- `discover_primary_services(conn_handle)` - Discover services
- `read_value_of_characteristic_using_value_handle(conn_handle, value_handle)` - Read characteristic

### BLE::GattDatabase

- `BLE::GattDatabase.new { block }` - Create GATT database
- `add_service(type, uuid) { block }` - Add service
- `add_characteristic(properties, uuid, permissions, initial_value) { block }` - Add characteristic
- `add_descriptor(properties, uuid, value)` - Add descriptor

### BLE::AdvertisingData

- `BLE::AdvertisingData.build { block }` - Build advertising data
- `add(type, *data)` - Add data field

## Common UUIDs and Constants

- `BLE::GATT_PRIMARY_SERVICE_UUID` - Primary service
- `BLE::GAP_SERVICE_UUID` - GAP service
- `BLE::GATT_SERVICE_UUID` - GATT service

### Characteristic Properties

- `BLE::READ` - Readable
- `BLE::WRITE` - Writable with response
- `BLE::WRITE_WITHOUT_RESPONSE` - Writable without response
- `BLE::NOTIFY` - Notifiable
- `BLE::INDICATE` - Indicatable

### Permissions

- `BLE::READ_ANYBODY` - Anyone can read
- `BLE::READ_ENCRYPTED` - Requires encryption
- `BLE::WRITE_ANYBODY` - Anyone can write
- `BLE::WRITE_ENCRYPTED` - Requires encryption

## Notes

- Requires BTstack library
- Complex API - refer to examples for common patterns
- Supports BLE 4.0 and above features
