require 'uart'
require 'io/console'

# Print every line that arrives on UART0, without polling for it.
# PicoRuby (the mruby runtime) only; see the note at the bottom.
#
# To try it on a single board, jumper GPIO 0 (TX) to GPIO 1 (RX): the
# two lines written below come straight back, and the handler wakes to
# print them. Otherwise attach whatever is talking to the board and it
# will print what that sends.
#
# Received lines go to the console, not back out of the UART. Echoing
# them to the same UART would be a trap under loopback: every line would
# come back, be echoed again with another prefix, and grow without bound
# until the buffer overflowed.

uart = UART.new(txd_pin: 0, rxd_pin: 1, baudrate: 115200)

uart_irq = uart.irq(UART::RX_RECEIVE) do |u, event_type|
  # One call can stand for any number of interrupts -- a burst of
  # bytes, several lines, or none at all if an earlier read already
  # took everything. So drain until there is nothing left rather than
  # assuming the call means exactly one line.
  while line = u.gets
    print "received: #{line}"
  end
end

uart.puts "first line"
uart.puts "second line"

# The handler now fires whenever bytes arrive -- no polling interval
# to tune, nothing waking up while the line is quiet. This task just
# waits for a key; the handler keeps printing meanwhile.
IRQ.start
puts "Watching the UART; press any key to stop"
STDIN.getch

# Winding down: stop delivery, then drop this run's registration --
# on R2P2 the VM outlives the script, and a leftover registration
# would stack up on the next run.
IRQ.stop
uart_irq.unregister

# Note:
# The bytes belong to the unit, not to this UART object. A second
# UART.new on the same unit reads the same stream, and whatever has
# already arrived is still waiting in it.
#
# This example is PicoRuby (mruby) only: on FemtoRuby or a build
# without the event bridge (ESP32), UART#irq raises
# NotImplementedError -- poll the read API there instead.
