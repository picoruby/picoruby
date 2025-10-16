# uptime - show how long the system has been running

begin
  uptime_ms = VM.tick
  uptime_sec = uptime_ms / 1000
  uptime_min = uptime_sec / 60
  uptime_hour = uptime_min / 60
  uptime_day = uptime_hour / 24

  # Format output
  print "up "

  if uptime_day > 0
    print "#{uptime_day} day"
    print "s" if uptime_day > 1
    print ", "
  end

  if uptime_hour > 0
    hours = uptime_hour % 24
    print "#{hours} hour"
    print "s" if hours != 1
    print ", "
  end

  minutes = uptime_min % 60
  print "#{minutes} minute"
  print "s" if minutes != 1

  puts

  # Show time if available
  if Time.respond_to?(:now)
    puts "Current time: #{Time.now}"
  end
rescue => e
  puts "uptime: #{e.message}"
end
