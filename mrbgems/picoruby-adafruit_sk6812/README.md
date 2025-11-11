# picoruby-adafruit_sk6812

SK6812 RGB LED library for PicoRuby (uses RMT for precise timing).

## Usage

```ruby
require 'sk6812'

led = SK6812.new(25)  # GPIO pin 25

# Set color (RGB values: 0-255)
led.output(r: 255, g: 0, b: 0)    # Red
led.output(r: 0, g: 255, b: 0)    # Green
led.output(r: 0, g: 0, b: 255)    # Blue
led.output(r: 255, g: 255, b: 0)  # Yellow
led.output(r: 0, g: 0, b: 0)      # Off
```

## API

### Methods

- `SK6812.new(gpio_pin)` - Initialize SK6812 LED on specified GPIO pin
- `output(r: 0, g: 0, b: 0)` - Set LED color with RGB values (0-255)

## Notes

- Requires RMT (Remote Control) peripheral for precise timing
- Color order is GRB internally (handled automatically)
- Each color value ranges from 0 to 255
