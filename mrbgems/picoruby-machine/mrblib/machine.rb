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
end
