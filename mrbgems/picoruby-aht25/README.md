# picoruby-aht25

AHT25 temperature and humidity sensor library for PicoRuby.

## Usage

```ruby
require 'aht25'

i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 4, scl_pin: 5)
sensor = AHT25.new(i2c: i2c)

data = sensor.read
puts "Temperature: #{data[:temperature]}°C"
puts "Humidity: #{data[:humidity]}%"
```

## API

### Methods

- `AHT25.new(i2c:)` - Initialize sensor with I2C instance
- `read()` - Read sensor data, returns hash with `:temperature` and `:humidity` keys
- `check()` - Check sensor status, returns true if ready
- `reset()` - Reset sensor

## Notes

- I2C address is 0x38 (fixed)
- Temperature range: -40°C to 85°C
- Humidity range: 0% to 100%RH
