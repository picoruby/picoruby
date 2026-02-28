# BLE HID Keyboard Example
#
# Simple 2x2 matrix keyboard over BLE.
# Uses KeyboardMatrix for scanning and BLE::HID for output.
#
# Note: For full keyboard features (layers, tap/hold, combos),
# Keyboard#start has its own event loop which conflicts with BLE::HID's loop.
# A future Keyboard#scan method will enable full integration.
# For now, use KeyboardMatrix directly for BLE keyboards.

require 'ble'
require 'keyboard_matrix'

# GPIO pins for 2x2 matrix
ROW_PINS = [6, 7]
COL_PINS = [28, 27]

# Simple keymap (no layers)
KEYMAP = [
  0x04, 0x05,  # A, B
  0x06, 0x07   # C, D
]

matrix = KeyboardMatrix.new(ROW_PINS, COL_PINS)
hid = BLE::HID.new(name: "PicoKB")
hid.debug = true

hid.start do
  event = matrix.scan
  if event
    row = event[:row]
    col = event[:col]
    keycode = KEYMAP[row * COL_PINS.size + col] || 0
    if event[:pressed]
      hid.keyboard_send(0, keycode)
    else
      hid.keyboard_release
    end
  end
end
