require 'irq'

button = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)

led = GPIO.new(16, GPIO::OUT)

button.irq(GPIO::EDGE_FALL, debounce: 100, capture: {led: led}) do |button, event, cap|
  puts "Button pressed, toggling LED"
  cap[:led].write(cap[:led].high? ? 0 : 1)
end

# Main loop
loop do
  IRQ.process
  sleep_ms(10)
end

# Note:
# For mruby/c's technical reasons, local variables outside
# the irq block are not accessible from the callback.
# `capture` is a way to pass local variables into the block.
#
# If you can use mruby-based PicoRuby (MicroRuby), you can
# write the code simply like below:
# ```mruby
# led = GPIO.new(16, GPIO::OUT)
# button.irq(GPIO::EDGE_FALL, debounce: 100) do |button, event|
#   puts "Button pressed, toggling LED"
#   led.write(led.high? ? 0 : 1)
# end
# ```
