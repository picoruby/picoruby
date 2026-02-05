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
  LT(1, KC_SPACE), KC_A, KC_B, MT(KC_LSFT, KC_ENTER)
])

# Layer 1: Number layer (accessed via future MO key if needed)
kb.add_layer(:layer1, [
  KC_NO, KC_1, KC_2, KC_NO
])

# Start keyboard scanning
puts "Meishi2 keyboard starting..."
puts "Key layout:"
puts "  default: [Space/Layer1] [A]  [B]  [Enter/Shift]"
puts "  Tap [0,0]: Space key"
puts "  Hold [0,0]: Layer 1"
puts "  Tap [1, 1]: Enter key"
puts "  Hold [1, 1]: Left Shift key"
puts "Press keys to see output..."

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
