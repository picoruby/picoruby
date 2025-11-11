# picoruby-gpio

GPIO (General Purpose Input/Output) library for PicoRuby.

## Usage

```ruby
# Output mode
led = GPIO.new(25, GPIO::OUT)
led.write(1)  # HIGH
led.write(0)  # LOW

# Input mode with pull-up
button = GPIO.new(15, GPIO::IN | GPIO::PULL_UP)
if button.high?
  puts "Button is HIGH"
end

# Class methods (without GPIO instance)
GPIO.write_at(25, 1)
value = GPIO.read_at(15)
```

## API

### Constants

- `GPIO::IN` - Input mode
- `GPIO::OUT` - Output mode
- `GPIO::HIGH_Z` - High impedance mode
- `GPIO::PULL_UP` - Enable pull-up resistor
- `GPIO::PULL_DOWN` - Enable pull-down resistor
- `GPIO::OPEN_DRAIN` - Open drain mode
- `GPIO::ALT` - Alternate function mode

### Instance Methods

- `GPIO.new(pin, flags, alt_function = nil)` - Initialize GPIO pin
- `write(value)` - Write 0 or 1 to the pin
- `read()` - Read pin value (returns 0 or 1)
- `high?()` - Check if pin is HIGH
- `low?()` - Check if pin is LOW
- `setmode(flags, alt_function = nil)` - Change pin mode

### Class Methods

- `GPIO.read_at(pin)` - Read pin value without creating instance
- `GPIO.write_at(pin, value)` - Write to pin without creating instance
- `GPIO.high_at?(pin)` - Check if pin is HIGH
- `GPIO.low_at?(pin)` - Check if pin is LOW
