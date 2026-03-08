# Gherkin keyboard with DRb WebSocket server
#
# Hardware: PiPi Gherkin (Raspberry Pi Pico)
# Physical matrix: 5 rows x 6 cols = 30 keys
#
# Pin assignment (USB port on the right side):
#   Rows: GPIO 12, 11, 10, 9, 8  (row0 .. row4)
#   Cols: GPIO  7,  6,  5, 4, 3, 2  (col0 .. col5)
#
# Usage:
#   picoruby gherkin_drb.rb
#
# DRb WebSocket server listens on ws://0.0.0.0:9090
# Connect from browser: ws://device-ip:9090
#
# Remote API (via DRb::DRbObject):
#   kb.layers              => {default: [...], fn1: [...], ...}
#   kb.add_layer(name, kc) => updates keymap live

require 'keyboard'
require 'drb'
require 'cyw43'

include USB::HID::Keycode
include LayerKeycode

ROW_PINS = [12, 11, 10, 9, 8]
COL_PINS = [7, 6, 5, 4, 3, 2]

kb = Keyboard.new(ROW_PINS, COL_PINS)
kb.tap_threshold_ms = 200

# Register IP address macro (hold C + Q = type IP address)
# Typing the IP address into the browser's keymap_editor.html URI field
IP_MACRO = kb.add_macro(CYW43.ipv4_address)

# Layer indices: 0=default, 1=fn1, 2=fn2, 3=fn3, 4=fn4
FN1_SPC  = LT(1, KC_SPACE)   # tap: Space,     hold: layer 1
FN2_BSPC = LT(2, KC_BSPACE)  # tap: Backspace, hold: layer 2
FN3_C    = LT(3, KC_C)       # tap: C,         hold: layer 3
FN4_V    = LT(4, KC_V)       # tap: V,         hold: layer 4
CTL_Z    = MT(KC_LCTL, KC_Z) # tap: Z,         hold: Left Ctrl
ALT_X    = MT(KC_LALT, KC_X) # tap: X,         hold: Left Alt
ALT_N    = MT(KC_LALT, KC_N) # tap: N,         hold: Left Alt
CTL_M    = MT(KC_LCTL, KC_M) # tap: M,         hold: Left Ctrl
SFT_ENT  = S(KC_ENTER)       # Shift+Enter

# Layer 0: default
#   Q      W      E      R      T      Y      U      I      O      P
#   A      S      D      F      G      H      J      K      L      ESC
#   Z/CTL  X/ALT  C/fn3  V/fn4  BSP/f2 SPC/f1 B      N/ALT  M/CTL  S+ENT
kb.add_layer(:default, [
  KC_Q,  KC_W,  KC_E,  KC_R,  KC_T,     KC_Y,    KC_U,  KC_I,  KC_O,  KC_P,
  KC_A,  KC_S,  KC_D,  KC_F,  KC_G,     KC_H,    KC_J,  KC_K,  KC_L,  KC_ESC,
  CTL_Z, ALT_X, FN3_C, FN4_V, FN2_BSPC, FN1_SPC, KC_B,  ALT_N, CTL_M, SFT_ENT
])

# Layer 1: numbers and function keys (hold Space)
#   1      2      3      4      5      6      7      8      9      0
#   F1     F2     F3     F4     F5     F6     F7     F8     F9     F10
#   ---    ---    ---    ---    DEL    ---    ---    ---    ---    ---
kb.add_layer(:fn1, [
  KC_1,  KC_2,  KC_3,  KC_4,  KC_5,      KC_6,  KC_7,  KC_8,  KC_9,  KC_0,
  KC_F1, KC_F2, KC_F3, KC_F4, KC_F5,     KC_F6, KC_F7, KC_F8, KC_F9, KC_F10,
  KC_NO, KC_NO, KC_NO, KC_NO, KC_DELETE, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO
])

# Layer 2: symbols (hold Backspace)
#   !      @      #      $      %      ^      &      *      (      )
#   F11    F12    ---    ---    ---    ---    ---    ---    ---    `
#   ---    ---    ---    ---    ---    ---    ---    ---    ---    ---
kb.add_layer(:fn2, [
  S(KC_1), S(KC_2), S(KC_3), S(KC_4), S(KC_5), S(KC_6), S(KC_7), S(KC_8), S(KC_9), S(KC_0),
  KC_F11,  KC_F12,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_GRAVE,
  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO
])

# Layer 3: punctuation and arrows (hold C)
#   IP     ---    ---    ---    ---    -      =      [      ]      \
#   TAB    ---    ---    ---    ---    ,      .      /      ;      '
#   ---    ---    ---    ---    ---    ---    LEFT   DOWN   UP     RIGHT
# IP_MACRO (Q position): hold C then press Q to type the device IP address
kb.add_layer(:fn3, [
  IP_MACRO, KC_NO, KC_NO, KC_NO, KC_NO, KC_MINUS, KC_EQUAL, KC_LBRACKET, KC_RBRACKET, KC_BSLASH,
  KC_TAB, KC_NO, KC_NO, KC_NO, KC_NO, KC_COMMA, KC_DOT,   KC_SLASH,    KC_SCOLON,   KC_QUOTE,
  KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,    KC_LEFT,  KC_DOWN,     KC_UP,       KC_RIGHT
])

# Layer 4: shifted punctuation and navigation (hold V)
#   ---    ---    ---    ---    ---    _      +      {      }      |
#   TAB    ---    ---    ---    ---    <      >      ?      :      "
#   ---    ---    ---    ---    ---    ---    HOME   PGDN   PGUP   END
kb.add_layer(:fn4, [
  KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, S(KC_MINUS), S(KC_EQUAL), S(KC_LBRACKET), S(KC_RBRACKET), S(KC_BSLASH),
  KC_TAB, KC_NO, KC_NO, KC_NO, KC_NO, S(KC_COMMA), S(KC_DOT),   S(KC_SLASH),    S(KC_SCOLON),   S(KC_QUOTE),
  KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,       KC_HOME,     KC_PGDOWN,      KC_PGUP,        KC_END
])

# Start DRb WebSocket server as a background task.
# The cooperative scheduler yields to this task during kb.start I/O waits.
Task.new(name: "drb") do
  puts "DRb WebSocket server starting on ws://0.0.0.0:9090"
  DRb.start_service('ws://0.0.0.0:9090', kb)
  DRb.thread.join
end

puts "Keyboard starting..."
kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
