# Split Keyboard Example - Master Side
#
# This example demonstrates how to implement a split keyboard
# where the master side holds the entire keymap and receives
# key events from the slave side via UART.
#
# Architecture:
#   - Master: Runs Keyboard class, holds complete keymap
#   - Slave: Runs KeyboardMatrix only, sends raw events via UART
#
# Example configuration:
#   - Left side (master): 2 rows x 3 cols
#   - Right side (slave): 2 rows x 3 cols
#   - Combined keymap: 2 rows x 6 cols

require 'keyboard'
require 'uart'

include USB::HID::Keycode
include LayerKeycode

# Protocol: 1-byte encoding
# Bit 7: pressed (1) / released (0)
# Bits 4-6: row (0-7)
# Bits 0-3: col (0-15)
def parse_slave_data(byte)
  pressed = (byte & 0x80) != 0
  row = (byte >> 4) & 0x07
  col = byte & 0x0F
  [row, col, pressed]
end

# Master side: 2x3 matrix, but keymap covers 2x6 (both sides)
kb = Keyboard.new([0, 1], [2, 3, 4], keymap_cols: 6)

# Full keymap: left side (cols 0-2) + right side (cols 3-5)
kb.add_layer(:default, [
  # Left (master)         Right (slave)
  KC_A,   KC_B,   KC_C,   KC_D,   KC_E,   KC_F,
  KC_1,   MO(1),  KC_3,   KC_4,   MO(1),  KC_6
])

kb.add_layer(:function, [
  KC_F1,  KC_F2,  KC_F3,  KC_F4,  KC_F5,  KC_F6,
  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO
])

# Initialize UART for slave communication
uart = UART.new(unit: 0, txd: 8, rxd: 9, baudrate: 115200)

# Column offset for slave keys (slave cols 0-2 map to keymap cols 3-5)
SLAVE_COL_OFFSET = 3

kb.start do |event|
  # Check for incoming slave data
  while byte = uart.read_nonblock
    row, col, pressed = parse_slave_data(byte)
    # Inject slave event with column offset
    kb.inject_event(row, col + SLAVE_COL_OFFSET, pressed)
  end

  # Send USB HID report
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
