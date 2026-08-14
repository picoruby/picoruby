# picoruby-median_filter

Median filter for rejecting outlier samples from noisy sensors.

Real-world sensors occasionally report a completely wrong value: an
ultrasonic ranger misses an echo, an ADC catches a transient, a ToF
sensor glances off a moving edge. Linear filters (moving average, IIR)
cannot remove such spikes --- they only smear them into the output.
A median filter drops any minority of outliers entirely, at the cost of
a small, predictable latency.

## Usage

```ruby
require 'median_filter'

filter = MedianFilter.new # window: 3

filter.update(20)  # => 20
filter.update(20)  # => 20
filter.update(95)  # => 20  (spike rejected)
filter.update(21)  # => 21
```

A practical example with the HC-SR04 ultrasonic distance sensor
(`picoruby-hcsr04`), which reports a bogus distance every few
measurements:

```ruby
require 'hcsr04'
require 'median_filter'

sensor = HCSR04.new(trig: 7, echo: 6)
filter = MedianFilter.new

while true
  begin
    cm = filter.update(sensor.distance_cm)
    puts "#{cm} cm"
  rescue HCSR04::TimeoutError
    filter.reset # nothing in range; forget stale samples
  end
  sleep_ms 200
end
```

## API

- `MedianFilter.new(window: 3)` --- `window` must be a positive odd
  Integer. A window of N tolerates up to `(N - 1) / 2` consecutive
  outliers and delays a genuine change by the same number of samples.
- `#update(value)` --- feeds an Integer or Float sample and returns the
  median of the retained samples. Until the window fills up, the median
  of the samples seen so far is returned, so the first sample passes
  through as is.
- `#reset` --- forgets all retained samples. Call it when the signal
  source restarts (e.g. a sensor coming back into range).
- `#size` --- number of currently retained samples.
- `#window` --- the configured window size.
- `#window=` --- changes the window size on the fly (same validation as
  `new`). Shrinking discards the oldest retained samples immediately.

## Choosing between MedianFilter and IIRFilter

- Occasional wrong values (spikes, dropouts): use `picoruby-median_filter`.
- Continuous small jitter around the true value: use `picoruby-iir_filter`
  or chain it after a median filter.

## License

MIT
