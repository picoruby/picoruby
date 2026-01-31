# Simple keyboard example with layer switching
require 'keyboard_layer'
require 'usb/hid'

include Keycode
include LayerKeycode

# Pin configuration
ROW_PINS = [0, 1, 2]
COL_PINS = [3, 4, 5]

# Initialize keyboard
kb = KeyboardLayer.new(ROW_PINS, COL_PINS, debounce_time: 5)

# Add base layer
# Layout:
#  ESC  1     Fn (tap for Space, hold for layer 1)
#  TAB  Q     ToggleNum
#  CTRL A     S
kb.add_layer(:base, [
  KC_ESC,  KC_1,    MO(1, KC_SPC),  # Tap: Space, Hold: Layer 1
  KC_TAB,  KC_Q,    TG(2),
  KC_LCTL, KC_A,    KC_S
])

# Add function layer (accessed via Fn key)
# Layout:
#  `    F1    (transparent)
#  (t)  HOME  UP
#  (t)  LEFT  DOWN
kb.add_layer(:function, [
  KC_GRV,  KC_F1,   KC_NO,
  KC_NO,   KC_HOME, KC_UP,
  KC_NO,   KC_LEFT, KC_DOWN
])

# Add numpad layer (toggled)
# Layout:
#  (t)  7     8
#  (t)  4     (transparent)
#  (t)  1     2
kb.add_layer(:numpad, [
  KC_NO,   KC_7,    KC_8,
  KC_NO,   KC_4,    KC_NO,
  KC_NO,   KC_1,    KC_2
])

# Set up key event handler
kb.on_key_event do |event|
  puts "Key event: row=#{event[:row]} col=#{event[:col]} " \
       "keycode=0x#{event[:keycode].to_s(16)} pressed=#{event[:pressed]}"

  if event[:pressed]
    USB::HID.keyboard_send(event[:modifier], event[:keycode])
  else
    USB::HID.keyboard_release
  end
end

# Start keyboard scanning
puts "Starting keyboard with layers..."
puts "- [0,2]: Tap for Space, Hold for Fn layer (momentary)"
puts "- [1,2]: Toggle numpad layer"
kb.start
