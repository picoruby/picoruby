# picoruby-pwm

PWM (Pulse Width Modulation) library for PicoRuby.

## Usage

```ruby
# Initialize PWM on pin 15 at 1 kHz, half on
pwm = PWM.new(15, frequency: 1000, duty: 50)

# Change frequency (Hz)
pwm.frequency(2000)

# Change duty cycle (percent, 0 - 100)
pwm.duty(75)  # 75% duty cycle

# Set period in microseconds
pwm.period_us(1000)  # 1ms period = 1kHz

# Set pulse width in microseconds
pwm.pulse_width_us(500)  # 0.5ms pulse width

# Stop the output
pwm.frequency(0)

# LED dimming example
led = PWM.new(25, frequency: 1000, duty: 0)
i = 0
while i < 100
  led.duty(i)
  sleep_ms 10
  i += 1
end
```

## API

### Methods

- `PWM.new(pin, frequency: 0, duty: 50)` - Initialize PWM on the given pin.
  The default frequency of 0 leaves the output stopped until you set one.
- `frequency(freq)` - Set frequency in Hz; returns it. Zero stops the output.
- `duty(duty)` - Set duty cycle in percent; returns the value actually set
  (the argument clamped to 0 - 100).
- `period_us(microseconds)` - Set the period; returns the resulting frequency
  in Hz. The period must be positive.
- `pulse_width_us(microseconds)` - Set the pulse width against the frequency
  already in effect; returns the resulting duty in percent.

## Notes

- Duty cycle is a percentage between 0 and 100, not a fraction.
- Frequency is in Hz (cycles per second).
- Two channels share one PWM slice (GPIO `n` and `n + 1`, and `n + 16`), and a
  slice has a single counter: setting the frequency on one of them sets it for
  the other, and stopping one stops both.
- On the RP2040 and RP2350 the divider is chosen per frequency, so the duty
  resolution is as fine as the hardware allows at that frequency. The lowest
  frequency a slice can produce is `sys_clk / 255 / 65536` (about 7.5 Hz at
  125 MHz); asking for less gives you that.
