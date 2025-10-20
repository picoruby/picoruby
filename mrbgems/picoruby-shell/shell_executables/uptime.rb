# uptime - show how long the system has been running

begin
  uptime_sec = Machine.uptime_us / 1000_000
  now = Time.now
  uptime_min = uptime_sec / 60
  uptime_hour = uptime_min / 60
  uptime_day = uptime_hour / 24

  # Format output
  print "up "

  if 0 < uptime_day
    print "#{uptime_day} day"
    print "s" if 1 < uptime_day
    print ", "
  end

  if 0 < uptime_hour
    hours = uptime_hour % 24
    print "#{hours} hour"
    print "s" if 1 < hours
    print ", "
  end

  minutes = uptime_min % 60
  print "#{minutes} minute"
  print "s" if 1 < minutes

  secods = uptime_sec % 60
  print ", #{secods} second"
  print "s" if 1 < secods

  puts

  puts "Current time: #{now}"
rescue => e
  puts "uptime: #{e.message}"
end
