# Direct wiring keyboard example
# No matrix scanning needed - each button is a GPIO pin

require 'keyboard'

include USB::HID::Keycode
include LayerKeycode

# Pin configuration for direct wiring
# Each button maps to one row - no matrix columns needed
BUTTON_PINS = [17,18,19,20,21,22]

# Initialize keyboard
kb = Keyboard.new(BUTTON_PINS, [])

# Default layer: one keycode per physical button
kb.layer do
  row KC_LEFT
  row KC_DOWN
  row KC_UP
  row KC_RIGHT
  row KC_A
  row KC_B
end

puts "Direct wiring keyboard starting..."
puts "PINS: #{BUTTON_PINS.join(', ')}"

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
