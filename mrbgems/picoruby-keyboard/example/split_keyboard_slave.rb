# Split Keyboard Example - Slave Side
#
# This example demonstrates the slave side of a split keyboard.
# The slave only scans its local matrix and sends raw events to
# the master via UART. No keymap processing happens on the slave.
#
# Architecture:
#   - Slave: Runs KeyboardMatrix only, sends raw events
#   - Master: Receives events and processes with full keymap

require 'keyboard_matrix'
require 'uart'

# Protocol: 1-byte encoding
# Bit 7: pressed (1) / released (0)
# Bits 4-6: row (0-7)
# Bits 0-3: col (0-15)
def encode_event(event)
  byte = 0
  byte |= 0x80 if event[:pressed]
  byte |= (event[:row] & 0x07) << 4
  byte |= event[:col] & 0x0F
  byte
end

# Slave side: 2x3 matrix
matrix = KeyboardMatrix.new([0, 1], [2, 3, 4])
matrix.debounce_ms = 5

# Initialize UART for master communication
# Note: txd/rxd are swapped compared to master
uart = UART.new(unit: 0, txd: 9, rxd: 8, baudrate: 115200)

loop do
  if event = matrix.scan
    # Send event to master
    uart.write(encode_event(event))
  end
  sleep_ms(1)
end
