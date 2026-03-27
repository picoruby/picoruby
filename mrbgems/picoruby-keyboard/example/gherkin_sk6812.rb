# Gherkin keyboard + SK6812MINI-E RGB LED example (mruby)
#
# Combines gherkin.rb keyboard with sk6812.rb LED animation.
# 30 keys with 30 RGB LEDs showing a rotating rainbow pattern.
#
# Pin assignment (USB port on the right side):
#   Keyboard rows: GPIO 12, 11, 10, 9, 8
#   Keyboard cols: GPIO  7,  6,  5,  4, 3, 2
#   SK6812 data:   GPIO  0
#
# The LED animation runs inside the keyboard's on_tick callback,
# updating every 50 ticks (~50 ms) to avoid FIFO underrun.

require 'keyboard'
require 'pio'

include USB::HID::Keycode
include LayerKeycode

# --- Keyboard setup ---

ROW_PINS = [12, 11, 10, 9, 8]
COL_PINS = [7, 6, 5, 4, 3, 2]

kb = Keyboard.new(ROW_PINS, COL_PINS)
kb.tap_threshold_ms = 200

FN1_SPC  = LT(1, KC_SPACE)
FN2_BSPC = LT(2, KC_BSPACE)
FN3_C    = LT(3, KC_C)
FN4_V    = LT(4, KC_V)
CTL_Z    = MT(KC_LCTL, KC_Z)
ALT_X    = MT(KC_LALT, KC_X)
ALT_N    = MT(KC_LALT, KC_N)
CTL_M    = MT(KC_LCTL, KC_M)
SFT_ENT  = MT(KC_LSFT, KC_ENTER)

# Layer 0: default
kb.layer do
  row KC_Q,  KC_W,  KC_E,  KC_R,  KC_T,    KC_Y,    KC_U, KC_I,  KC_O,  KC_P
  row KC_A,  KC_S,  KC_D,  KC_F,  KC_G,    KC_H,    KC_J, KC_K,  KC_L,  KC_ESC
  row CTL_Z, ALT_X, FN3_C, FN4_V, FN2_BSPC,FN1_SPC, KC_B, ALT_N, CTL_M, SFT_ENT
end

# Layer 1: numbers and function keys (hold Space)
kb.layer(:fn1) do
  row KC_1,  KC_2,  KC_3,  KC_4,  KC_5,     KC_6,  KC_7,  KC_8,  KC_9,  KC_0
  row KC_F1, KC_F2, KC_F3, KC_F4, KC_F5,    KC_F6, KC_F7, KC_F8, KC_F9, KC_F10
  row KC_NO, KC_NO, KC_NO, KC_NO, KC_DELETE,KC_NO, KC_NO, KC_NO, KC_NO, KC_NO
end

# Layer 2: symbols and function keys (hold Backspace)
kb.layer(:fn2) do
  row S(KC_1), S(KC_2), S(KC_3), S(KC_4), S(KC_5), S(KC_6), S(KC_7), S(KC_8), S(KC_9), S(KC_0)
  row KC_F11,  KC_F12,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_GRAVE
  row KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO
end

# Layer 3: punctuation and arrows (hold C)
kb.layer(:fn3) do
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_MINUS, KC_EQUAL, KC_LBRACKET,KC_RBRACKET,KC_BSLASH
  row KC_TAB, KC_NO, KC_NO, KC_NO, KC_NO, KC_COMMA, KC_DOT,   KC_SLASH,   KC_SCOLON,  KC_QUOTE
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,    KC_LEFT,  KC_DOWN,    KC_UP,      KC_RIGHT
end

# Layer 4: shifted punctuation and navigation (hold V)
kb.layer(:fn4) do
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, S(KC_MINUS), S(KC_EQUAL), S(KC_LBRACKET),S(KC_RBRACKET),S(KC_BSLASH)
  row KC_TAB, KC_NO, KC_NO, KC_NO, KC_NO, S(KC_COMMA), S(KC_DOT),   S(KC_SLASH),   S(KC_SCOLON),  S(KC_QUOTE)
  row KC_NO,  KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,       KC_HOME,     KC_PGDOWN,     KC_PGUP,       KC_END
end

# --- SK6812 LED setup ---

NUM_LEDS = 30
LED_PIN = 0
BRIGHTNESS = 32  # 0-255 (keep low to avoid eye strain)

# PIO program for WS2812/SK6812 protocol
# Each bit takes 10 cycles at 8 MHz = 1250 ns
#   bit 1: HIGH 875 ns (7 cy), LOW 375 ns (3 cy)
#   bit 0: HIGH 250 ns (2 cy), LOW 1000 ns (8 cy)
sk6812 = PIO.asm(side_set: 1) do
  wrap_target
  label :bitloop
  out :x, 1,                   side: 0, delay: 2
  jmp :do_zero, cond: :not_x,  side: 1, delay: 1
  label :do_one
  jmp :bitloop,                side: 1, delay: 4
  label :do_zero
  nop                          side: 0, delay: 4
  wrap
end

sm = PIO::StateMachine.new(
  pio: PIO::PIO0, sm: 0,
  program: sk6812, freq: 8_000_000,
  sideset_pins: LED_PIN,
  out_shift_right: false,
  out_shift_autopull: true,
  out_shift_threshold: 24,
  fifo_join: PIO::FIFO_JOIN_TX
)
sm.start

# Color wheel: phase 0-255 -> GRB value for 24-bit autopull
def pixel_color(phase)
  if phase < 85
    r = (255 - phase * 3) * BRIGHTNESS / 255
    g = (phase * 3) * BRIGHTNESS / 255
    b = 0
  elsif phase < 170
    ph = phase - 85
    r = 0
    g = (255 - ph * 3) * BRIGHTNESS / 255
    b = (ph * 3) * BRIGHTNESS / 255
  else
    ph = phase - 170
    r = (ph * 3) * BRIGHTNESS / 255
    g = 0
    b = (255 - ph * 3) * BRIGHTNESS / 255
  end
  ((g << 16) | (r << 8) | b) << 8
end

# Pre-compute rainbow pixel data
pixels = []
i = 0
while i < NUM_LEDS
  pixels << pixel_color(i * 256 / NUM_LEDS)
  i += 1
end

led_buf = Array.new(NUM_LEDS, 0)
led_offset = 0
led_tick = 0
LED_INTERVAL = 50  # update LEDs every 50 ticks (~50 ms)

# Send initial frame
i = 0
while i < NUM_LEDS
  led_buf[i] = pixels[i]
  i += 1
end
sm.put_buffer(led_buf)

# --- LED animation via on_tick ---

kb.on_tick do |_keyboard|
  led_tick += 1
  if LED_INTERVAL <= led_tick
    led_tick = 0
    led_offset = (led_offset + 1) % NUM_LEDS
    i = 0
    while i < NUM_LEDS
      led_buf[i] = pixels[(i + led_offset) % NUM_LEDS]
      i += 1
    end
    sm.put_buffer(led_buf)
  end
end

# --- Start keyboard (main loop) ---

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
