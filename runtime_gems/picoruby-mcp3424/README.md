# picoruby-mcp3424

MCP3424 18-bit ADC (Analog-to-Digital Converter) library for PicoRuby.

## Usage

```ruby
require 'mcp3424'

i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 4, scl_pin: 5)
adc = MCP3424.new(i2c: i2c, address_selector: 0)

# Configure resolution and gain
adc.bit_resolution = 16  # 12, 14, 16, or 18 bits
adc.pga_gain = 1         # 1, 2, 4, or 8

# One-shot reading
value = adc.one_shot_read(1)  # Read channel 1
puts "ADC value: #{value}"

# Continuous conversion
adc.start_continuous_conversion(1)
loop do
  value = adc.read
  puts "ADC: #{value}" if value
  sleep 0.1
end
```

## API

### Methods

- `MCP3424.new(i2c:, address_selector:)` - Initialize ADC (address_selector: 0-3)
- `bit_resolution=(bits)` - Set resolution (12, 14, 16, or 18 bits)
- `pga_gain=(gain)` - Set programmable gain amplifier (1, 2, 4, or 8)
- `one_shot_read(channel, timeout_ms = nil)` - Read one sample from channel (1-4)
- `start_continuous_conversion(channel)` - Start continuous conversion on channel
- `read(timeout_ms = nil)` - Read current ADC value
- `set_address(address_selector)` - Change I2C address

## Notes

- Base I2C address: 0x68 (configurable with address_selector 0-3)
- 4 differential input channels
- Higher resolution = longer conversion time
