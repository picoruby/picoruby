require "watchdog"

puts

if Watchdog.caused_reboot?
  puts "Reboot by watchdog!"
else
  puts "Boot at first time!"
end

sleep 1

Watchdog.enable(3000) # 3 seconds

5.times do
  puts "I'm being watched!"
  sleep 1
  Watchdog.update
end

puts "I'll reboot in 3 seconds!"

while true
  sleep 1
  puts "I'm not being watched!"
end
