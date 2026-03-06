module Machine
  $_signal_self_manage = false

  def self.reboot(wait_ms = 0)
    if Machine.mcu_name == 'POSIX'
      sleep_ms wait_ms
      self._reboot
    else
      Watchdog.reboot(wait_ms)
    end
  rescue => e
    puts "error: reboot is not available on this platform"
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
