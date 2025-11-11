# picoruby-adc

ADC (Analog-to-Digital Converter) library for PicoRuby.

## Usage

```ruby
# Initialize ADC on pin 26
adc = ADC.new(26)

# Read as float (0.0 - 1.0)
value = adc.read
puts "ADC value: #{value}"

# Read as voltage
voltage = adc.read_voltage
puts "Voltage: #{voltage}V"

# Read raw value (depends on ADC resolution)
raw = adc.read_raw
puts "Raw ADC: #{raw}"
```

## API

### Methods

- `ADC.new(pin, additional_params = {})` - Initialize ADC on specified pin
- `read()` - Read ADC value as Float (0.0 - 1.0)
- `read_voltage()` - Read ADC value as voltage
- `read_raw()` - Read raw ADC value as Integer

## Notes

- Pin number depends on the target board (e.g., RP2040 has ADC on pins 26-29)
- The `additional_params` hash can contain platform-specific configuration
