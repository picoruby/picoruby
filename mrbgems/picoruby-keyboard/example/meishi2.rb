# Meishi2 keyboard example
#
# Hardware: Meishi2 (2x2 matrix)
# - Rows: GPIO 6, 7
# - Cols: GPIO 28, 27
#
# (ref): PRK firmware keymap: https://github.com/picoruby/prk_meishi2

require 'keyboard'

include USB::HID::Keycode
include LayerKeycode

# Pin configuration for Meishi2
ROW_PINS = [6, 7]
COL_PINS = [28, 27]

# Initialize keyboard
kb = Keyboard.new(ROW_PINS, COL_PINS, debounce_ms: 40)
# Tap threshold: 200ms (PRK uses different thresholds per key, but we use global setting)
kb.tap_threshold_ms = 200

kb.add_layer(:default, [
  MO(1, KC_SPACE), KC_A, KC_B, KC_LSFT
])

# Layer 1: Accessed by holding key[0,0]
kb.add_layer(:layer1, [
  MO(1, KC_SPACE), KC_1, KC_2, KC_LSFT
])

# Start keyboard scanning
puts "Meishi2 keyboard starting..."
puts "Key layout:"
puts "  default: [Space/L1] [A]  [B]  [Shift]"
puts "  layer1:  [Space/L1] [1]  [2]  [Shift]"

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
