# picoruby-adafruit_pcf8523

PCF8523 Real-Time Clock (RTC) library for PicoRuby.

## Usage

```ruby
require 'pcf8523'

i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 4, scl_pin: 5)
rtc = PCF8523.new(i2c: i2c)

# Set current time
rtc.current_time = Time.local(2024, 1, 15, 10, 30, 0)

# Read current time
time = rtc.current_time
puts "#{time.year}/#{time.mon}/#{time.mday} #{time.hour}:#{time.min}:#{time.sec}"
```

## API

### Methods

- `PCF8523.new(i2c:)` - Initialize RTC with I2C instance
- `current_time=(time)` - Set RTC time using Time object
- `current_time()` - Read current time from RTC, returns Time object
- `set_power_management(pow = 0b001)` - Configure power management mode

## Notes

- I2C address is 0x68 (fixed)
- Supported year range: 2000-2099
- Battery backup support with configurable power management
- Default power mode enables battery switch-over in direct switching mode
