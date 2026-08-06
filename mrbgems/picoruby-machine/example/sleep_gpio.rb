require 'machine'
require 'irq'

# Sleep until a button press. Wiring: a button between GPIO 15 and
# GND, pull-up enabled below; the falling edge wakes the machine and
# execution continues right after the call.
#
# The GPIO event constants (EDGE_FALL etc.) come from the irq gem.

button = GPIO.new(15, GPIO::IN | GPIO::PULL_UP)

puts "Sleeping until the button on GPIO 15 is pressed..."

# deep: true is DORMANT: lowest power, but USB is re-enumerated on
# wake (reopen the terminal). Use deep: false to keep the console
# alive through the sleep.
Machine.sleep(deep: true, source: button, level: GPIO::EDGE_FALL)

puts "Woke up!"
