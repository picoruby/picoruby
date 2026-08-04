require 'irq'
require 'io/console'

# PicoRuby (the mruby runtime) version: register a handler, call
# IRQ.start, done -- no polling loop, no cleanup machinery. For
# FemtoRuby (mruby/c), which polls with IRQ.process instead, see
# irq_gpio_femtoruby.rb.

button = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)
led = GPIO.new(16, GPIO::OUT)

# This sample is not suitable for reliable mechanical-switch input because it
# does not debounce the switch. Contact bounce may cause the LED to flicker.
# Debounce in hardware or confirm that the input is stable in software instead.
led.write(button.low? ? 1 : 0)

# On PicoRuby the block sees the surrounding local variables, so no
# capture is needed.
button_irq = button.irq(GPIO::EDGE_FALL | GPIO::EDGE_RISE) do |btn, _event|
  led.write(btn.low? ? 1 : 0)
end

# The handler now runs whenever the button fires; this task has
# nothing left to do but wait. Without the wait, the script itself
# would end here and only the handler would linger in the background
# -- fine for firmware, confusing on the R2P2 shell.
IRQ.start
puts "Watching the button; press any key to stop"
STDIN.getch

# Winding down: stop delivery, then drop this run's registration --
# on R2P2 the VM outlives the script, and a leftover registration
# would stack up on the next run.
IRQ.stop
button_irq.unregister
led.write(0)
