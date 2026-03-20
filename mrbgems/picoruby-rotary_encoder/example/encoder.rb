require 'rotary_encoder'

enc = RotaryEncoder.new(20, 21)

enc.clockwise do
  puts "CW"
end

enc.counterclockwise do
  puts "CCW"
end

loop do
  IRQ.process
  sleep_ms(1)
end
