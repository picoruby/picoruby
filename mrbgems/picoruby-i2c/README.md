# picoruby-i2c

I2C (Inter-Integrated Circuit) communication library for PicoRuby.

## Usage

```ruby
# Initialize I2C
i2c = I2C.new(unit: :RP2040_I2C0, frequency: 400_000, sda_pin: 4, scl_pin: 5)

# On RP2040 the unit can be omitted; it is inferred from the pins
i2c = I2C.new(sda_pin: 4, scl_pin: 5)  # inferred as :RP2040_I2C0

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

- `I2C.new(unit: nil, frequency: DEFAULT_FREQUENCY, sda_pin:, scl_pin:, timeout: DEFAULT_TIMEOUT)` - Initialize I2C. On RP2040 `unit:` is optional and inferred from `sda_pin`/`scl_pin`. If a given `unit:` disagrees with the pins, or the pins imply different units, or no unit can be determined, an `ArgumentError` is raised. On ESP32 `unit:` is still required.
- `write(address, *data, timeout: nil, nostop: false)` - Write data to I2C device
- `read(address, length, *write_data, timeout: nil)` - Read data from I2C device
- `scan(timeout: nil)` - Scan and print connected I2C devices

## Notes

- Address should be 7-bit I2C address (not 8-bit with R/W bit)
- Data can be Integer, String, or Array of Integers

## nRF52 notes

- Units are `NRF52_I2C0` and `NRF52_I2C1`, mapping to TWIM0 and TWIM1.
  `NRF52_TWIM0` / `NRF52_TWIM1` are accepted as aliases.
- Any pin can carry SDA or SCL: the peripheral selects pins from its own
  side, so there is no fixed pin table as on RP2040.
- TWIM0 shares its hardware with SPIM0 and TWIS0 (and likewise for unit
  1), so one unit cannot serve I2C and SPI at the same time. Nothing
  detects the clash; give I2C and SPI different unit numbers.
- Only 100 kHz, 250 kHz and 400 kHz exist. A request is rounded down to
  one of those where it can be, so 300 kHz gives 250 kHz. 100 kHz is the
  floor -- a slower request still runs at 100 kHz because the hardware has
  nothing below it -- and 400 kHz is the ceiling.
- Internal pull-ups are enabled, but at roughly 13 kOhm they are too weak
  for anything but a short, lightly loaded bus. Fit external pull-ups.
- `sda_pin:` and `scl_pin:` are required; there is no board default.
- Transfers are limited to 65535 bytes, and addresses to 7 bits.
