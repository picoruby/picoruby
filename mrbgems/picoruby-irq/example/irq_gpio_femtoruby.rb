require 'irq'

# FemtoRuby (mruby/c) version: events are polled with IRQ.process.
# IRQ.start is not available there -- FemtoRuby cannot spawn the
# dispatcher task -- so the main loop keeps asking. For PicoRuby (the
# mruby runtime), which delivers without a loop, see
# irq_gpio_picoruby.rb.

button = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)
led = GPIO.new(16, GPIO::OUT)

# This sample is not suitable for reliable mechanical-switch input because it
# does not debounce the switch. Contact bounce may cause the LED to flicker.
# Debounce in hardware or confirm that the input is stable in software instead.
led.write(button.low? ? 1 : 0)

# On FemtoRuby, local variables outside the irq block are not
# accessible from the callback, for technical reasons of that VM.
# `capture` is the way to pass them in.
button.irq(GPIO::EDGE_FALL | GPIO::EDGE_RISE, capture: {led: led}) do |btn, _event, cap|
  cap[:led].write(btn.low? ? 1 : 0)
end

loop do
  IRQ.process  # dispatches queued events to their handlers
  sleep_ms 10
end
