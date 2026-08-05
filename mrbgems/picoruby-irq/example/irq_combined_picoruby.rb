require 'irq'
require 'uart'
require 'io/console'

# Two peripherals, one switch. Every handler registered on any
# peripheral is delivered through the same hidden dispatcher task once
# IRQ.start runs -- adding a handler never adds a loop, a task, or a
# queue to the user program. PicoRuby (the mruby runtime) only.
#
# Wiring: a button between GPIO 17 and GND (pull-up enabled below),
# and a jumper from GPIO 0 (TX) to GPIO 1 (RX) so the UART talks to
# itself.
#
# The handlers deliberately interact: pressing the button writes a
# line out of the UART, the loopback brings it back, and the UART
# handler prints it. All handlers run in the one dispatcher task, so
# no two of them ever run at the same time, and state they share --
# here the uart object -- needs no locking.

button = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)
uart = UART.new(txd_pin: 0, rxd_pin: 1, baudrate: 115200)

# No debounce, so a bouncy switch prints more than once per press; see
# the note in irq_gpio_picoruby.rb.
button_irq = button.irq(GPIO::EDGE_FALL) do |_btn, _event|
  uart.puts "pressed"
end

uart_irq = uart.irq(UART::RX_RECEIVE) do |u, _event|
  # One call may stand for a burst, so drain rather than count.
  while line = u.gets
    print "received: #{line}"
  end
end

IRQ.start
puts "Press the button, or press any key here to stop"
STDIN.getch

# On R2P2 the VM outlives the script, so drop this run's
# registrations; a leftover would stack up on the next run.
IRQ.stop
button_irq.unregister
uart_irq.unregister
