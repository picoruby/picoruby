require 'hcsr04'

hcsr04 = HCSR04.new(echo: 0, trig: 1)

while true
  begin
    puts "Distance: #{hcsr04.distance_cm} cm"
  rescue StandardError
    puts "Distance: Out of range"
  end
  sleep 1
end
