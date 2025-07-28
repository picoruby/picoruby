require 'gpio'

# HC-SR04 Ultrasonic Distance Sensor
#
# Usage:
#
# require 'hcsr04'
# hc = HCSR04.new(trig: 26, echo: 27)
# hc.distance_cm # => returns distance in centimeters
#
# HCSR04#distance_cm should not be called too frequently.
# 200 ms is the minimum interval between calls.
#
# ┌────────────────────────────────────────────────────┐
# │ HC-SR04 Timing Diagram                             │
# ├────────────────────────────────────────────────────┤
# │         ┌────┐                                     │
# │         │    │ (>10μs pulse)                       │
# │ TRIG  ──┘    └───────────────────────────────      │
# │                                                    │
# │ 40kHz            ││││││││         ││││││││         │
# │ Sonic ───────────┴┴┴┴┴┴┴┴─────────┴┴┴┴┴┴┴┴───      │
# │                  Burst            Reflection       │
# │                  ┌────────────────┐                │
# │                  │                │ pulse_width    │
# │ ECHO  ───────────┘                └──────────      │
# │                                                    │
# │ Time  ──┬────┬───┬────────────────┬────────        │
# │         A    B   C                D                │
# │                                                    │
# │ Output A: Send trigger pulse                       │
# │        B: Stop trigger pulse (B - A > 10μs)        │
# │ Input  C: Echo starts (40kHz sonic burst is sent)  │
# │        D: Echo ends   (40kHz sonic is reflected)   │
# │                                                    │
# │ Distance = (D to C duration) × sound_speed ÷ 2     │
# │          = pulse_width × 343m/s ÷ 2                │
# └────────────────────────────────────────────────────┘

class HCSR04

  class TimeoutError < StandardError; end

  class WraparoundError < StandardError; end

  TIMEOUT_USEC = 30_000 # Timeout in 30 milliseconds = 514 cm max

  def initialize(trig:, echo:)
    @trig = GPIO.new(trig, GPIO::OUT)
    @echo = GPIO.new(echo, GPIO::IN)
  end

  def distance_cm
    count ||= 0

    # Const and ivar are comparably slow, so we use local variables
    timeout_usec = TIMEOUT_USEC
    echo = @echo

    @trig.write 1 # A in timing diagram
    sleep_ms 1
    @trig.write 0 # B in timing diagram

    # We want to avoid calculating in Float, which is very slow in RP2040
    # Instead, we use Time#usec to get the microseconds fraction of the current time.
    start_time_usec = Time.now.usec
    while echo.low?
      diff = Time.now.usec - start_time_usec
      if diff < 0
        raise WraparoundError
      elsif timeout_usec < diff
        raise TimeoutError.new("Echo signal not received within #{timeout_usec} us")
      end
    end

    # C in timing diagram (40kHz sonic burst goes)
    echo_start_usec = Time.now.usec

    while echo.high?
      diff = Time.now.usec - echo_start_usec
      # @type var diff: Integer
      if diff < 0
        raise WraparoundError
      elsif timeout_usec < diff
        raise TimeoutError.new("Echo signal timeout within #{timeout_usec} us")
      end
    end

    # D in timing diagram (40kHz sonic reflection comes)
    duration_usec = Time.now.usec - echo_start_usec

    if duration_usec < 0
      # This happens when the time wraps around
      duration_usec += 1_000_000
    end

    return duration_usec * 1_715 / 100_000

  rescue WraparoundError
    # @type var count: Integer
    count += 1
    if 3 < count
      retry
    else
      raise TimeoutError.new("Time wrapped around too many times")
    end
  end

end
