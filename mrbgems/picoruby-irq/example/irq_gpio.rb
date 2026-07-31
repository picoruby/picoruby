require 'irq'

module IRQGPIOExample
  def self.install_cleanup(button_irq, led, source)
    @button_irq = button_irq
    @led = led
    @source = source
    @previous_int_handler = Signal.trap(:INT) { IRQGPIOExample.cleanup }
  end

  def self.cleanup
    @button_irq.unregister
    @led.write(0)
    IRQ.take(@source) # release a token that may still be outstanding
    IRQ.unbind(@source)
    @button_irq = nil
    @led = nil
    @source = nil
    previous = @previous_int_handler
    @previous_int_handler = nil
    puts "Exiting..."
    Signal.trap(:INT, previous || "DEFAULT")
  end
end

button = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)
led = GPIO.new(16, GPIO::OUT)

# This sample is not suitable for reliable mechanical-switch input because it
# does not debounce the switch. Contact bounce may cause the LED to flicker.
# Debounce in hardware or confirm that the input is stable in software instead.
led.write(button.low? ? 1 : 0)
button_irq = button.irq(GPIO::EDGE_FALL | GPIO::EDGE_RISE, capture: {led: led}) do |button, _event, cap|
  cap[:led].write(button.low? ? 1 : 0)
end

# Main loop. IRQ.bind arranges for a token to arrive whenever a GPIO
# interrupt has queued something, and pop blocks until then, so there is
# no polling interval to tune and nothing to wake up for while the
# button is idle.
q = Task::Queue.new
source = IRQ.gpio_source
IRQ.bind(source, q)
IRQGPIOExample.install_cleanup(button_irq, led, source)

loop do
  source = q.pop
  # Taking the bits is required: it is what allows the next token.
  IRQ.take(source)
  # One token can stand for any number of interrupts, so keep
  # dispatching until there is nothing left rather than assuming the
  # token means exactly one event.
  while 0 < IRQ.process
  end
end

# Note:
# On FemtoRuby (the mruby/c-based runtime), local variables outside
# the irq block are not accessible from the callback, for technical
# reasons of that VM. `capture` is a way to pass local variables into
# the block.
#
# On PicoRuby (the mruby-based runtime) you can
# write the code simply like below:
# ```mruby
# led = GPIO.new(16, GPIO::OUT)
# led.write(button.low? ? 1 : 0)
# button.irq(GPIO::EDGE_FALL | GPIO::EDGE_RISE) do |button, _event|
#   led.write(button.low? ? 1 : 0)
# end
# ```
