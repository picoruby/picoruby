# Staggered keyboard example
# 5-row row-staggered layout (standard keyboard style, no function row)

require 'keyboard'
require 'drb'
require 'cyw43'

include USB::HID::Keycode
include LayerKeycode

ROW_PINS = [0, 1, 2, 3, 4, 5]
COL_PINS = [6, 7, 8, 9, 10, 11, 12, 13, 14, 15]

kb = Keyboard.new(ROW_PINS, COL_PINS)

kb.layer do
  row KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINUS, KC_EQUAL, KC_BSPACE
  row KC_TAB, X(0.3), KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBRACKET, KC_RBRACKET, KC_BSLASH
  row KC_CAPS, X(0.5), KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCOLON, KC_QUOTE, KC_ENTER
  row KC_LSFT, X(1), KC_Z, KC_X, KC_C, KC_X, KC_B, KC_N, KC_M, KC_COMMA,KC_DOT, KC_SLASH, KC_RSFT
  row KC_LCTL, X(0.5), KC_LALT, KC_SPACE, X(5.5), KC_RALT, KC_LEFT, KC_DOWN, KC_UP, KC_RIGHT
end

  puts "DRb WebSocket server starting on ws://0.0.0.0:9090"
  DRb.start_service('ws://0.0.0.0:9090', kb)
  DRb.thread.join
#kb.start do |event|
# USB::HID.keyboard_send(event[:modifier], event[:keycode])
#end
