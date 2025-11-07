# uptime - show how long the system has been running

begin
  caused_reboot = Watchdog.caused_reboot? ? 'yes' : 'no'
rescue NameError
  caused_reboot = nil
end

begin
  puts Time.now.inspect
  puts "up #{Machine.uptime_formatted}"
  puts "Watchdog rebooted: #{caused_reboot}" if caused_reboot
rescue => e
  puts "uptime: #{e.message}"
end
