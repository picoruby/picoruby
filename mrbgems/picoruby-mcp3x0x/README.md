# picoruby-mcp3x0x

MCP3004/MCP3008/MCP3204/MCP3208 SPI ADC library for PicoRuby.

## Usage

```ruby
require 'mcp3x0x'

# MCP3008: 8-channel 10-bit ADC
adc = MCP3008.new(
  unit: :RP2040_SPI0,
  sck_pin: 18,
  cipo_pin: 16,
  copi_pin: 19,
  cs_pin: 17
)

# Single-ended read
value = adc.read(0)  # Read channel 0 (0-7)
puts "ADC: #{value}"

# Differential read
diff = adc.read(0, differential: true)  # CH0(+) - CH1(-)
```

## Supported Chips

- **MCP3004**: 4-channel, 10-bit ADC
- **MCP3008**: 8-channel, 10-bit ADC
- **MCP3204**: 4-channel, 12-bit ADC
- **MCP3208**: 8-channel, 12-bit ADC

## API

### Methods

- `MCP300x.new(unit:, frequency: 500000, sck_pin:, cipo_pin:, copi_pin:, cs_pin:)` - Initialize ADC
- `read(channel, differential: false)` - Read ADC value from channel

### Channels

**Single-ended mode** (differential: false):
- MCP3004/MCP3204: channels 0-3
- MCP3008/MCP3208: channels 0-7

**Differential mode** (differential: true):
- Channel 0: CH0(+) - CH1(-)
- Channel 1: CH0(-) - CH1(+)
- Channel 2: CH2(+) - CH3(-)
- Channel 3: CH2(-) - CH3(+)
- (MCP3008/MCP3208 have additional pairs for CH4-7)

## Notes

- Default SPI frequency: 500kHz
- MCP3004/MCP3008: 10-bit resolution (0-1023)
- MCP3204/MCP3208: 12-bit resolution (0-4095)
