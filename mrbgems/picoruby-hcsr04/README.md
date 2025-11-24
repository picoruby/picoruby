# picoruby-hcsr04

HC-SR04 ultrasonic distance sensor library for PicoRuby.

## Usage

```ruby
require 'hcsr04'

sensor = HCSR04.new(trig: 26, echo: 27)
distance = sensor.distance_cm
puts "Distance: #{distance} cm"
```

## API

### Methods

- `HCSR04.new(trig:, echo:)` - Initialize sensor with trigger and echo pins
- `distance_cm()` - Read distance in centimeters

## Notes

- Do not call `distance_cm` too frequently (minimum 200ms interval recommended)
- Maximum detection range is approximately 400cm
- Returns distance based on ultrasonic pulse reflection time
