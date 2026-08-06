module Machine
  $_signal_self_manage = false

  def self.posix?
    # Platform name is defined in picoruby-require
    !%w(RP2040 RP2350 ESP32).include?(RUBY_PLATFORM)
  end

  def self.reboot(wait_ms = 0)
    if Object.const_defined?(:Watchdog)
      if Watchdog.respond_to?(:reboot)
        Watchdog.reboot(wait_ms)
      else
        raise "reboot is not supported on this platform"
      end
    else
      sleep_ms wait_ms
      self._reboot
    end
  end

  def self.signal_self_manage
    $_signal_self_manage = true
  end

  def self.pop_signal_self_manage
    s = $_signal_self_manage
    $_signal_self_manage = false
    return s
  end

  def self.wifi_available?
    Object.const_defined?(:Network) && Network.const_defined?(:WiFi)
  end

  # Machine-level sleep: the WHOLE VM stops -- every task, the 1ms
  # tick, every kind of delivery -- until the wake source fires, then
  # execution continues right here. Task-level waiting is
  # Kernel#sleep, a different thing entirely.
  #
  #   Machine.sleep(deep: true,  source: :timer, ms: 2000)
  #   Machine.sleep(deep: false, source: pin, level: GPIO::EDGE_FALL)
  #
  # deep: false is SLEEP mode, true is DORMANT; per-chip depth and
  # power figures are in the README. source: :timer wakes after ms:
  # milliseconds; a GPIO source wakes on level:, one of the four GPIO
  # event constants. Returns nil; raises on anything it cannot do.
  def self.sleep(deep:, source:, **opt)
    unless true == deep || false == deep
      raise TypeError, "deep: must be true or false"
    end
    case source
    when :timer
      ms = opt.delete(:ms)
      unless opt.size == 0
        raise ArgumentError, "unknown option for source: :timer"
      end
      unless ms.is_a?(Integer)
        raise ArgumentError, "ms: must be an Integer"
      end
      # The C side takes uint32_t; reject instead of wrapping.
      if ms < 1 || 4294967295 < ms
        raise ArgumentError, "ms: out of range (1..4294967295)"
      end
      _sleep_timer(deep, ms)
    else
      # Duck typing on purpose: this gem must not depend on the gpio
      # gem, and any object that knows its pin can be a wake source.
      unless source.respond_to?(:pin)
        raise ArgumentError, "source: must be :timer or respond to #pin"
      end
      level = opt.delete(:level)
      unless opt.size == 0
        raise ArgumentError, "unknown option for a GPIO source"
      end
      pin = source.pin
      unless pin.is_a?(Integer)
        raise ArgumentError, "source.pin must be an Integer"
      end
      # The constants are defined by the irq gem (which reopens GPIO);
      # any caller writing level: GPIO::EDGE_FALL has it. Combined
      # masks are not accepted: the hardware wake condition is a
      # single (edge, high) pair.
      case level
      when GPIO::LEVEL_LOW  then _sleep_gpio(deep, pin, false, false)
      when GPIO::LEVEL_HIGH then _sleep_gpio(deep, pin, false, true)
      when GPIO::EDGE_FALL  then _sleep_gpio(deep, pin, true, false)
      when GPIO::EDGE_RISE  then _sleep_gpio(deep, pin, true, true)
      else
        raise ArgumentError, "level: must be one of GPIO::LEVEL_LOW, LEVEL_HIGH, EDGE_FALL, EDGE_RISE"
      end
    end
    nil
  end
end
