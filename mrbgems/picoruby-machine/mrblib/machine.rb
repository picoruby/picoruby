module Machine
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
end
