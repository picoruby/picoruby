# picoruby-uart

UART (Universal Asynchronous Receiver/Transmitter) serial communication library for PicoRuby.

## Usage

```ruby
# Initialize UART
uart = UART.new(
  unit: :RP2040_UART0,
  txd_pin: 0,
  rxd_pin: 1,
  baudrate: 115200
)

# Write data
uart.write("Hello, World!\n")
uart.puts("Hello")  # Adds line ending

# On RP2040 the unit can be omitted; it is inferred from the pins
uart = UART.new(txd_pin: 4, rxd_pin: 5)  # inferred as :RP2040_UART1

# Read data
data = uart.read(10)  # Read up to 10 bytes
data = uart.read      # Read all available data
line = uart.gets      # Read until line ending

# Check available bytes
if uart.bytes_available > 0
  byte = uart.getbyte
  received_at_us = uart.last_read_timestamp_us
end

# Detect bytes dropped because the RX ring buffer was full
dropped_bytes = uart.rx_overflow_count

# Clear buffers
uart.clear_rx_buffer
uart.clear_tx_buffer
```

## The receive buffer belongs to the unit

A UART unit has exactly one RX buffer, and it belongs to the unit rather
than to any one `UART` object. The interrupt handler writes into it, so
it has to outlive the objects that read from it.

Two consequences:

- Opening a unit that is already open reuses that buffer, so bytes which
  arrived earlier are still there. Two `UART` objects on one unit are two
  views of a single stream: a byte read through either is gone for both.
- `rx_buffer_size:` therefore only applies to the open that creates the
  buffer. Asking for a different size on a unit that is already open
  raises `ArgumentError`, rather than freeing a buffer the interrupt
  handler may be writing into.

## Waiting for input without polling

Every read method above returns `nil` when nothing has arrived, so a
loop that wants a line has to keep asking and sleeping between tries.
On PicoRuby (the mruby runtime), register a handler instead and let
the receive interrupt drive it:

```ruby
require 'uart'

uart = UART.new(txd_pin: 0, rxd_pin: 1, baudrate: 115200)

uart.irq(UART::RX_RECEIVE) do |u, event_type|
  # One call may stand for a burst -- several bytes, several lines, or
  # a leftover already read -- so drain rather than count.
  while line = u.gets
    print "received: #{line}"
  end
end

IRQ.start   # from here on, the handler fires; no polling loop
```

The handler runs in a hidden dispatcher task (see
[picoruby-irq](../picoruby-irq/README.md)), so whatever it touches is
shared state. `uart.irq` returns an instance with `disable` /
`enable` / `unregister`; `IRQ.stop` releases everything and lets a
finished program exit. `UART::RX_RECEIVE` is the only event today --
further conditions such as break detection become further bits, and a
handler registered for several receives the mask of what happened in
`event_type`.

Note that writing the reply back to the same UART is only safe with a
peer on the other end. Under a TX-to-RX loopback it feeds itself: every
line written comes back, is answered again, and grows without bound.

Each unit has its own event source, so a handler on `RP2040_UART0`
does not fire for traffic on `RP2040_UART1`.

`UART#irq` raises `NotImplementedError` on FemtoRuby (mruby/c), which
cannot spawn the dispatcher task, and on builds without the bridge
(ESP32). There, poll as above -- or on FemtoRuby use the queue-level
bridge API directly: `IRQ.bind(uart.event_source_id, queue)` delivers
a token per coalesced burst, as described in picoruby-irq's README.

See [example/uart_event_picoruby.rb](example/uart_event_picoruby.rb).

## API

### Constants

- `UART::PARITY_NONE` - No parity
- `UART::PARITY_ODD` - Odd parity
- `UART::PARITY_EVEN` - Even parity
- `UART::FLOW_CONTROL_NONE` - No flow control
- `UART::FLOW_CONTROL_RTS_CTS` - Hardware flow control
- `UART::FLOW_CONTROL_XON_XOFF` - Software flow control

### Methods

- `UART.new(unit: nil, txd_pin:, rxd_pin:, baudrate: 115200, data_bits: 8, stop_bits: 1, parity: PARITY_NONE, flow_control: FLOW_CONTROL_NONE, rx_buffer_size: nil)` - Initialize UART. On RP2040 `unit:` is optional and inferred from `txd_pin`/`rxd_pin` (only the pins you pass are considered, so RX-only or TX-only setups work). If a given `unit:` disagrees with the pins, or the pins imply different units, or no unit can be determined, an `ArgumentError` is raised. On ESP32 `unit:` is still required.
- `write(string)` - Write string to TX
- `putc(ch)` - Write the low 8 bits of an Integer or the first character of a String
- `puts(string)` - Write string with line ending
- `getbyte()` - Read 1 byte from RX
- `last_read_timestamp_us()` - Return the receive timestamp in microseconds for the byte most recently returned by `getbyte`, or `nil` before the first byte and after `clear_rx_buffer`. `read`, `readpartial` and `gets` do not update it.
- `rx_overflow_count()` - Return the cumulative number of bytes dropped because the RX ring buffer was full
- `read(length = nil)` - Read data from RX
- `gets()` - Read line (until line ending)
- `readpartial(maxlen)` - Read available data up to maxlen
- `bytes_available()` - Return number of bytes in RX buffer
- `irq(event_mask, capture: nil) { |uart, event_type, capture| }` - Register a handler for RX events, delivered after `IRQ.start`; see "Waiting for input without polling"
- `event_source_id()` - Return the event-bridge source for this unit, to pass to `IRQ.bind` (queue-level use). Only defined where the build has the bridge
- `line_ending=(ending)` - Set line ending ("\n", "\r\n", or "\r")
- `setmode(...)` - Change UART settings
- `clear_rx_buffer()` - Discard buffered input, and forget the last read timestamp with it
- `clear_tx_buffer()` - Clear transmit buffer
- `flush()` - Wait for TX to complete
