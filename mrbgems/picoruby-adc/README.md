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
- On nRF52840 only P0.02-P0.05 and P0.28-P0.31 reach the converter; there
  is no mux to point another GPIO at it, so any other pin is rejected.
  Full scale is 3.6 V (internal 0.6 V reference at 1/6 gain) and readings
  are 12-bit. `ADC.new(:temperature)` is not available: the die sensor is
  a separate peripheral reporting quarter-degrees, not a voltage.
- The `additional_params` hash can contain platform-specific configuration
