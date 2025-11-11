# picoruby-pwm

PWM (Pulse Width Modulation) library for PicoRuby.

## Usage

```ruby
# Initialize PWM on pin 15
pwm = PWM.new(15, frequency: 1000, duty: 0.5)

# Change frequency (Hz)
pwm.frequency(2000)

# Change duty cycle (0.0 - 1.0)
pwm.duty(0.75)  # 75% duty cycle

# Set period in microseconds
pwm.period_us(1000)  # 1ms period = 1kHz

# Set pulse width in microseconds
pwm.pulse_width_us(500)  # 0.5ms pulse width

# LED dimming example
led = PWM.new(25, frequency: 1000, duty: 0.0)
100.times do |i|
  led.duty(i / 100.0)
  sleep 0.01
end
```

## API

### Methods

- `PWM.new(pin, frequency: 1000, duty: 0.0)` - Initialize PWM on specified pin
- `frequency(freq)` - Set frequency in Hz
- `duty(duty)` - Set duty cycle (0.0 - 1.0)
- `period_us(microseconds)` - Set period in microseconds
- `pulse_width_us(microseconds)` - Set pulse width in microseconds

## Notes

- Duty cycle is a value between 0.0 (0%) and 1.0 (100%)
- Frequency is in Hz (cycles per second)
