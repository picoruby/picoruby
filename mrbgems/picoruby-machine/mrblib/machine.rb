module Machine
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

  def self.wifi_available?
    Object.const_defined?(:Network) && Network.const_defined?(:WiFi)
  end
end
