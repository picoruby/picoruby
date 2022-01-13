# Initialize a Keyboard
kbd = Keyboard.new

# `split=` should happen before `init_pins`
kbd.split = true

# Caution!!!!
#   - Keyboard#mutual_uart_at_my_own_risk is experimental.
#     It may make the keyboard unstable and may break your chip
#   - Make sure to disconnect the TRS(TRRS) cable when you change
#     this configuration. Reconnect them after changing BOTH halves
#   - Keys RGB_xxx are working on partner half only using this option
kbd.mutual_uart_at_my_own_risk = true

# If your right hand of CRKBD is the "anchor"
kbd.set_anchor(:right)

# Initialize GPIO assign
kbd.init_pins(
  [ 4, 5, 6, 7 ],            # row0, row1,... respectively
  [ 29, 28, 27, 26, 22, 20 ] # col0, col1,... respectively
)

# default layer should be added at first
kbd.add_layer :default, %i[
  KC_ESCAPE KC_Q    KC_W    KC_E        KC_R    KC_T      KC_Y      KC_U      KC_I      KC_O     KC_P      KC_MINUS
  KC_TAB    KC_A    KC_S    KC_D        KC_F    KC_G      KC_H      KC_J      KC_K      KC_L     KC_SCOLON KC_BSPACE
  KC_LSFT   KC_Z    KC_X    KC_C        KC_V    KC_B      KC_N      KC_M      KC_COMMA  KC_DOT   KC_SLASH  KC_RSFT
  KC_NO     KC_NO   KC_NO   ALT_AT      KC_LCTL LOWER_SPC RAISE_ENT SPC_CTL   RUBY_GUI  KC_NO    KC_NO     KC_NO
]
kbd.add_layer :raise, %i[
  KC_GRAVE  KC_EXLM KC_AT   KC_HASH     KC_DLR  KC_PERC   KC_CIRC   KC_AMPR   KC_ASTER  KC_LPRN  KC_RPRN   KC_EQUAL
  KC_TAB    KC_LABK KC_LCBR KC_LBRACKET KC_LPRN KC_QUOTE  KC_LEFT   KC_DOWN   KC_UP     KC_RIGHT KC_UNDS   KC_PIPE
  KC_LSFT   KC_RABK KC_RCBR KC_RBRACKET KC_RPRN KC_DQUO   KC_TILD   KC_BSLASH KC_COMMA  KC_DOT   KC_SLASH  KC_RSFT
  KC_NO     KC_NO   KC_NO   ALT_AT      KC_LCTL ADJUST    RAISE_ENT SPC_CTL   KC_RGUI   KC_NO    KC_NO     KC_NO
]
kbd.add_layer :lower, %i[
  KC_ESCAPE KC_1    KC_2    KC_3        KC_4    KC_5      KC_6      KC_7      KC_8      KC_9     KC_0      KC_EQUAL
  KC_TAB    KC_NO   KC_NO   KC_NO       KC_LPRN KC_QUOTE  KC_DOT    KC_4      KC_5      KC_6     KC_PLUS   KC_BSPACE
  KC_LSFT   KC_RABK KC_RCBR KC_RBRACKET KC_RPRN KC_DQUO   KC_0      KC_1      KC_2      KC_3     KC_SLASH  KC_COMMA
  KC_NO     KC_NO   KC_NO   ALT_AT      KC_LCTL LOWER_SPC ADJUST    SPC_CTL   KC_RGUI   KC_NO    KC_NO     KC_NO
]
kbd.add_layer :adjust, %i[
  KC_F1     KC_F2   KC_F3   KC_F4       KC_F5   KC_F6     KC_F7     KC_F8     KC_F9     KC_F10   KC_F11    KC_F12
  RGB_TOG   RGB_SPI RGB_HUI RGB_SAI     RGB_VAI RGB_MOD   KC_NO     KC_NO     KC_NO     KC_NO    KC_NO     KC_NO
  RGB_TOG   RGB_SPD RGB_HUD RGB_SAD     RGB_VAD RGB_RMOD  KC_NO     KC_NO     KC_NO     KC_NO    KC_NO     KC_NO
  KC_NO     KC_NO   KC_NO   ALT_AT      KC_LCTL ADJUST    ADJUST    SPC_CTL   KC_RGUI   KC_NO    KC_NO     KC_NO
]

kbd.define_composite_key :SPC_CTL, %i(KC_SPACE KC_RCTL)

#
#                   Your custom     Keycode or             Keycode (only modifiers)    Release time     Re-push time
#                   key name        Array of Keycode       or Layer Symbol to be held  threshold(ms)    threshold(ms)
#                                   or Proc                or Proc which will run      to consider as   to consider as
#                                   when you click         while you keep press        `click the key`  `hold the key`
kbd.define_mode_key :ALT_AT,      [ :KC_AT,                :KC_LALT,                   150,             150 ]
kbd.define_mode_key :RAISE_ENT,   [ :KC_ENTER,             :raise,                     150,             150 ]
kbd.define_mode_key :LOWER_SPC,   [ :KC_SPACE,             :lower,                     150,             150 ]
kbd.define_mode_key :RUBY_GUI,    [ Proc.new { kbd.ruby }, :KC_RGUI,                   300,             nil ]
kbd.define_mode_key :ADJUST,      [ nil,                   :adjust,                    nil,             nil ]

# `before_report` will work just right before reporting what keys are pushed to USB host.
# You can use it to hack data by adding an instance method to Keyboard class by yourself.
# ex) Use Keyboard#before_report filter if you want to input `":" w/o shift` and `";" w/ shift`
#kbd.before_report do
#  kbd.invert_sft if kbd.keys_include?(:KC_SCOLON)
#  # You'll be also able to write `invert_ctl`, `invert_alt` and `invert_gui`
#end

# Initialize RGBLED with pin, underglow_size, backlight_size and is_rgbw.
rgb = RGB.new(
  0,    # pin number
  6,    # size of underglow pixel
  21,   # size of backlight pixel
  false # 32bit data will be sent to a pixel if true while 24bit if false
)
# Set an effect
#  `nil` or `:off` for turning off
rgb.effect = :swirl
# rgb.effect = :rainbow_mood
# Set an action when you input
#  `nil` or `:off` for turning off
# rgb.action = :thunder
# Append the feature. Will possibly be able to write `Keyboard#append(OLED.new)` in the future
kbd.append rgb

kbd.start!
