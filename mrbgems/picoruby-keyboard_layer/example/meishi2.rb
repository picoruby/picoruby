# Meishi2 keyboard example
#
# Hardware: Meishi2 (2x2 matrix)
# - Rows: GPIO 6, 7
# - Cols: GPIO 28, 27
#
# (ref): PRK firmware keymap: https://github.com/picoruby/prk_meishi2

require 'keyboard_layer'

include LayerKeycode

# Pin configuration for Meishi2
ROW_PINS = [6, 7]
COL_PINS = [28, 27]

# Initialize keyboard
kb = KeyboardLayer.new(ROW_PINS, COL_PINS, debounce_ms: 40)
# Tap threshold: 200ms (PRK uses different thresholds per key, but we use global setting)
kb.tap_threshold_ms = 200

kb.add_layer(:default, [
  MO(1, KC_SPACE), KC_A, KC_B, MO(2, KC_ENTER)
])

# Layer 1: Accessed by holding key[0,0]
kb.add_layer(:layer1, [
  MO(1, KC_SPACE), KC_1, KC_2, MO(2, KC_ENTER)
])

# Layer 2: Accessed by holding key[1,1]
kb.add_layer(:layer2, [
  MO(1, KC_SPACE), KC_F1, KC_F2, MO(2, KC_ENTER)
])

# Set up key event handler
kb.on_key_event do |event|
  if event[:pressed]
    USB::HID.keyboard_send(event[:modifier], event[:keycode])
  else
    USB::HID.keyboard_release
  end
end

# Start keyboard scanning
puts "Meishi2 keyboard starting..."
puts "Key layout:"
puts "  default: [Space/L1] [A]  [B]  [Enter/L2]"
puts "  layer1:  [Space/L1] [1]  [2]  [Enter/L2]"
puts "  layer2:  [Space/L1] [F1] [F2] [Enter/L2]"
kb.start
