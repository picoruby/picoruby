# Gherkin keyboard example (30-key ortholinear)
#
# Hardware: PiPi Gherkin (Raspberry Pi Pico)
# Physical matrix: 5 rows x 6 cols = 30 keys
#
# Pin assignment (USB port on the right side):
#   Rows: GPIO 12, 11, 10, 9, 8  (row0 .. row4)
#   Cols: GPIO  7,  6,  5, 4, 3, 2  (col0 .. col5)
#
# The keymap arrays below use visual 3x10 layout (matching the keyboard),
# NOT the 5x6 physical wiring order.
# Flat index mapping (scan order row0..row4 x col0..col5):
#   visual row 0 (indices  0- 9): Q  W  E  R  T  Y  U  I  O  P
#   visual row 1 (indices 10-19): A  S  D  F  G  H  J  K  L  ESC
#   visual row 2 (indices 20-29): Z  X  C  V  BSP SPC B  N  M  ENT
#
# Ported from KMK firmware keymap:
#   https://github.com/adafruit/Adafruit_Learning_System_Guides/blob/main/PB_Gherkin/code.py

require 'keyboard'

include USB::HID::Keycode
include LayerKeycode

ROW_PINS = [12, 11, 10, 9, 8]
COL_PINS = [7, 6, 5, 4, 3, 2]

kb = Keyboard.new(ROW_PINS, COL_PINS)
kb.tap_threshold_ms = 200

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
kb.layer do
  row KC_Q,  KC_W,  KC_E,  KC_R,  KC_T,    KC_Y,    KC_U, KC_I,  KC_O,  KC_P
  row KC_A,  KC_S,  KC_D,  KC_F,  KC_G,    KC_H,    KC_J, KC_K,  KC_L,  KC_ESC
  row CTL_Z, ALT_X, FN3_C, FN4_V, FN2_BSPC,FN1_SPC, KC_B, ALT_N, CTL_M, SFT_ENT
end

# Layer 1: numbers and function keys (hold Space)
#   1      2      3      4      5      6      7      8      9      0
#   F1     F2     F3     F4     F5     F6     F7     F8     F9     F10
#   ---    ---    ---    ---    DEL    ---    ---    ---    ---    ---
kb.layer(:fn1) do
  row KC_1,  KC_2,  KC_3,  KC_4,  KC_5,     KC_6,  KC_7,  KC_8,  KC_9,  KC_0
  row KC_F1, KC_F2, KC_F3, KC_F4, KC_F5,    KC_F6, KC_F7, KC_F8, KC_F9, KC_F10
  row KC_NO, KC_NO, KC_NO, KC_NO, KC_DELETE,KC_NO, KC_NO, KC_NO, KC_NO, KC_NO
end

# Layer 2: symbols and function keys (hold Backspace)
#   !      @      #      $      %      ^      &      *      (      )
#   F11    F12    ---    ---    ---    ---    ---    ---    ---    `
#   ---    ---    ---    ---    ---    ---    ---    ---    ---    ---
kb.layer(:fn2) do
  row S(KC_1), S(KC_2), S(KC_3), S(KC_4), S(KC_5), S(KC_6), S(KC_7), S(KC_8), S(KC_9), S(KC_0)
  row KC_F11,  KC_F12,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_GRAVE
  row KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO
end

# Layer 3: punctuation and arrows (hold C)
#   ---    ---    ---    ---    ---    -      =      [      ]      \
#   TAB    ---    ---    ---    ---    ,      .      /      ;      '
#   ---    ---    ---    ---    ---    ---    LEFT   DOWN   UP     RIGHT
kb.layer(:fn3) do
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_MINUS, KC_EQUAL, KC_LBRACKET,KC_RBRACKET,KC_BSLASH
  row KC_TAB, KC_NO, KC_NO, KC_NO, KC_NO, KC_COMMA, KC_DOT,   KC_SLASH,   KC_SCOLON,  KC_QUOTE
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,    KC_LEFT,  KC_DOWN,    KC_UP,      KC_RIGHT
end

# Layer 4: shifted punctuation and navigation (hold V)
#   ---    ---    ---    ---    ---    _      +      {      }      |
#   TAB    ---    ---    ---    ---    <      >      ?      :      "
#   ---    ---    ---    ---    ---    ---    HOME   PGDN   PGUP   END
kb.layer(:fn4) do
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, S(KC_MINUS), S(KC_EQUAL), S(KC_LBRACKET),S(KC_RBRACKET),S(KC_BSLASH)
  row KC_TAB, KC_NO, KC_NO, KC_NO, KC_NO, S(KC_COMMA), S(KC_DOT),   S(KC_SLASH),   S(KC_SCOLON),  S(KC_QUOTE)
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,       KC_HOME,     KC_PGDOWN,     KC_PGUP,       KC_END
end

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
