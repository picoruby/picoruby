# picoruby-i2c

I2C (Inter-Integrated Circuit) communication library for PicoRuby.

## Usage

```ruby
# Initialize I2C
i2c = I2C.new(unit: :RP2040_I2C0, frequency: 400_000, sda_pin: 4, scl_pin: 5)

# Write data
i2c.write(0x50, 0x00, 0x01, 0x02)  # Write to device 0x50

# Read data
data = i2c.read(0x50, 4)  # Read 4 bytes from device 0x50

# Write then read
data = i2c.read(0x50, 2, 0x00)  # Write 0x00, then read 2 bytes

# Scan for devices
i2c.scan  # Prints addresses of connected devices
```

## API

### Constants

- `I2C::DEFAULT_FREQUENCY` - Default I2C frequency
- `I2C::DEFAULT_TIMEOUT` - Default timeout value

### Methods

- `I2C.new(unit:, frequency: DEFAULT_FREQUENCY, sda_pin:, scl_pin:, timeout: DEFAULT_TIMEOUT)` - Initialize I2C
- `write(address, *data, timeout: nil, nostop: false)` - Write data to I2C device
- `read(address, length, *write_data, timeout: nil)` - Read data from I2C device
- `scan(timeout: nil)` - Scan and print connected I2C devices

## Notes

- Address should be 7-bit I2C address (not 8-bit with R/W bit)
- Data can be Integer, String, or Array of Integers
