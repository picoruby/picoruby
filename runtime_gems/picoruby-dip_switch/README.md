# picoruby-dip_switch

DIP switch reader library for PicoRuby.

## Usage

```ruby
require 'dip_switch'

# Positive logic (with control pin)
c_pin = GPIO.new(0, GPIO::OUT)
dip = DipSwitch.new(c_pin, [1, 2, 3, 4])
value = dip.read  # Returns 0..15 (4-bit value)

# Negative logic (without control pin)
dip = DipSwitch.new(nil, [1, 2, 3, 4])
value = dip.read  # Returns 0..15 (4-bit value)
```

## API

### Methods

- `DipSwitch.new(control_pin, pin_numbers)` - Initialize DIP switch reader
  - `control_pin`: GPIO instance for control (use `nil` for negative logic)
  - `pin_numbers`: Array of GPIO pin numbers for switch positions
- `read()` - Read DIP switch value as Integer

## Notes

- **Positive logic**: Uses pull-down resistors, requires control pin
- **Negative logic**: Uses pull-up resistors, no control pin needed
- Return value represents the binary state of all switches
- Example: 4 switches can return values 0-15
