require 'uart'
require 'irq'

# Print every line that arrives on UART0, without polling for it.
#
# To try it on a single board, jumper GPIO 0 (TX) to GPIO 1 (RX): the
# two lines written below come straight back, and the task wakes to
# print them. Otherwise attach whatever is talking to the board and it
# will print what that sends.
#
# Received lines go to the console, not back out of the UART. Echoing
# them to the same UART would be a trap under loopback: every line would
# come back, be echoed again with another prefix, and grow without bound
# until the buffer overflowed.

module UARTEventExample
  def self.install_cleanup(source)
    @source = source
    @previous_int_handler = Signal.trap(:INT) { UARTEventExample.cleanup }
  end

  def self.cleanup
    IRQ.take(@source) # release a token that may still be outstanding
    IRQ.unbind(@source)
    @source = nil
    previous = @previous_int_handler
    @previous_int_handler = nil
    puts "Exiting..."
    Signal.trap(:INT, previous || "DEFAULT")
  end
end

uart = UART.new(txd_pin: 0, rxd_pin: 1, baudrate: 115200)

# IRQ.bind arranges for a token to arrive whenever the receive interrupt
# has put something in the buffer, and pop blocks until then. There is no
# polling interval to tune, and nothing to wake up for while the line is
# quiet.
q = Task::Queue.new
source = uart.event_source_id
IRQ.bind(source, q)
UARTEventExample.install_cleanup(source)

uart.puts "first line"
uart.puts "second line"

loop do
  source = q.pop
  # Taking the bits is required: it is what allows the next token.
  IRQ.take(source)
  # One token can stand for any number of interrupts -- a burst of
  # bytes, several lines, or none at all if the last read already took
  # everything. So drain until there is nothing left rather than
  # assuming the token means exactly one line.
  while line = uart.gets
    print "received: #{line}"
  end
end

# Note:
# The bytes belong to the unit, not to this UART object. A second
# UART.new on the same unit reads the same stream, and whatever has
# already arrived is still waiting in it.
#
# On a build without the event bridge -- ESP32, or any build configured
# without a task scheduler -- UART#event_source_id does not exist, and
# the loop above has to become a poll:
# ```ruby
# loop do
#   while line = uart.gets
#     print "received: #{line}"
#   end
#   sleep_ms 50
# end
# ```
