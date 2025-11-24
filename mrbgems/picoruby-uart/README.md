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

# Read data
data = uart.read(10)  # Read up to 10 bytes
data = uart.read      # Read all available data
line = uart.gets      # Read until line ending

# Check available bytes
if uart.bytes_available > 0
  data = uart.read
end

# Clear buffers
uart.clear_rx_buffer
uart.clear_tx_buffer
```

## API

### Constants

- `UART::PARITY_NONE` - No parity
- `UART::PARITY_ODD` - Odd parity
- `UART::PARITY_EVEN` - Even parity
- `UART::FLOW_CONTROL_NONE` - No flow control
- `UART::FLOW_CONTROL_RTS_CTS` - Hardware flow control
- `UART::FLOW_CONTROL_XON_XOFF` - Software flow control

### Methods

- `UART.new(unit:, txd_pin:, rxd_pin:, baudrate: 115200, data_bits: 8, stop_bits: 1, parity: PARITY_NONE, flow_control: FLOW_CONTROL_NONE, rx_buffer_size: nil)` - Initialize UART
- `write(string)` - Write string to UART
- `puts(string)` - Write string with line ending
- `read(length = nil)` - Read data from UART
- `gets()` - Read line (until line ending)
- `readpartial(maxlen)` - Read available data up to maxlen
- `bytes_available()` - Return number of bytes in RX buffer
- `line_ending=(ending)` - Set line ending ("\n", "\r\n", or "\r")
- `setmode(...)` - Change UART settings
- `clear_rx_buffer()` - Clear receive buffer
- `clear_tx_buffer()` - Clear transmit buffer
- `flush()` - Wait for TX to complete
