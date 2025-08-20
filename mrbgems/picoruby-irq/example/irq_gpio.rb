require 'irq'

button = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)

$led = GPIO.new(16, GPIO::OUT)

button.irq(GPIO::EDGE_FALL, debounce: 100) do |gpio, event|
  puts "Button pressed, toggling LED"
  $led.write($led.high? ? 0 : 1)
end

# Main loop
loop do
  IRQ.process
  sleep_ms(10)
end

# Note:
# For mruby/c's technical reasons, local variables outside 
# the irq block are not accessible from the block.
# This is why we use `$led` global variable here.
# On the other hand, you can use `led` local variable in
# mruby-based PicoRuby (MicroRuby)
