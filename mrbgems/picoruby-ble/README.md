# picoruby-ble

Bluetooth Low Energy (BLE) library for PicoRuby - supports Central, Peripheral, Observer, and Broadcaster roles.

## Usage

An application subclasses `BLE`, overrides the callbacks it needs, and runs the event loop with `start` (peripheral, broadcaster) or `scan` (central).

### Peripheral (Server) Example

```ruby
require 'ble'

class MyPeripheral < BLE
  BTSTACK_EVENT_STATE = 0x60

  def initialize
    db = BLE::GattDatabase.new do |db|
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, 0x1234) do |s|
        s.add_characteristic(BLE::READ, 0x5678, BLE::READ, "Initial value")
      end
    end
    super(:peripheral, db.profile_data)
    @adv_data = BLE::AdvertisingData.build do |ad|
      ad.add(0x01, 0x06)        # Flags
      ad.add(0x09, "MyDevice")  # Complete local name
    end
  end

  # Called with every event that `start` pops from BTstack
  def packet_callback(event_packet)
    return unless event_packet.getbyte(0) == BTSTACK_EVENT_STATE
    advertise(@adv_data) if event_packet.getbyte(2) == BLE::HCI_STATE_WORKING
  end
end

MyPeripheral.new.start # Runs until Ctrl-C
```

### Central (Client) Example

```ruby
require 'ble'

class MyCentral < BLE
  def initialize
    super(:central)
  end

  # Called with every advertising report while scanning
  def advertising_report_callback(report)
    return unless report.name_include?("MyDevice")
    puts report.format
    connect(report) # The scan loop then discovers the services and stops
  end
end

central = MyCentral.new
central.scan(debug: true)
central.services.each do |service|
  puts sprintf("Service 0x%04X", service[:uuid32] || 0)
  service[:characteristics].each do |chara|
    puts sprintf("  Characteristic 0x%04X value: %s", chara[:uuid32] || 0, chara[:value].inspect)
  end
end
```

### Scanning Only

A central that never calls `connect` just reports what it hears:

```ruby
require 'ble'

class MyScanner < BLE
  def initialize
    super(:central)
  end

  def advertising_report_callback(report)
    puts "Device: #{report.format}"
    puts "RSSI: #{report.rssi}"
  end
end

MyScanner.new.scan(scan_type: :passive, timeout_ms: 10000)
```

## API

### BLE Class

- `BLE.new(role, profile_data = nil)` - Initialize BLE
  - `role`: `:central`, `:peripheral`, `:observer`, or `:broadcaster`
  - `profile_data`: GATT database for peripheral role

- `start(timeout_ms = nil, stop_state = :no_stop)` - Run the event loop: powers the controller on, dispatches events to `packet_callback` and `heartbeat_callback`, and powers it off when it returns (after `timeout_ms` milliseconds, when `state` reaches `stop_state`, or on Ctrl-C)
- `packet_callback(event_packet)` - Override to handle BTstack events
- `heartbeat_callback()` - Override; called about once a second while the loop runs
- `gap_local_bd_addr()` - Get local Bluetooth address
- `hci_power_control(power_mode)` - Control HCI power state

### Peripheral Methods

- `advertise(ad_data)` - Start advertising (call it once the controller is up, see the example)
- `notify(handle)` - Send notification
- `pop_write_value(handle)` - Get value written by client
- `push_read_value(handle, value)` - Set value for read request

### Central Methods

- `scan(scan_type:, scan_interval:, scan_window:, timeout_ms:, stop_state:, debug:)` - Scan for advertising devices; reports arrive via `advertising_report_callback`
- `advertising_report_callback(report)` - Override to receive each `BLE::AdvertisingReport`
- `connect(report)` - Connect to the device of an advertising report; call it from `advertising_report_callback`, the running scan loop then discovers the services into `services` and stops
- `services` - Discovered services, each with its characteristics and descriptors
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
