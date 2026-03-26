# SK6812MINI-E example: 30 LEDs rainbow animation on GPIO 0
#
# SK6812MINI-E uses GRB color order and is protocol-compatible with WS2812.
# PIO frequency: 8 MHz (125 ns/cycle, 10 cycles/bit = 1250 ns/bit)
#
# IMPORTANT: Pixel data must be pre-computed before sending.
# If computation happens inside the send loop, PicoRuby cannot keep up
# with PIO consumption rate and TX FIFO underruns, causing the SK6812
# to see an unintended reset (LOW > 80 us).

NUM_LEDS = 30
GPIO_PIN = 0
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
  sideset_pins: GPIO_PIN,
  out_shift_right: false,
  out_shift_autopull: true,
  out_shift_threshold: 24,
  fifo_join: PIO::FIFO_JOIN_TX
)
sm.start

# Color wheel: phase 0-255 -> RGB, scaled by BRIGHTNESS
# Returns GRB value left-shifted by 8 for 24-bit autopull
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

# All LEDs off on exit
Signal.trap(:INT) do
  i = 0
  while i < NUM_LEDS
    sm.put(0)
    i += 1
  end
end

# Animation loop: rotate the rainbow pattern
offset = 0
while true
  # Send all pixels as fast as possible to prevent FIFO underrun
  i = 0
  while i < NUM_LEDS
    sm.put(pixels[(i + offset) % NUM_LEDS])
    i += 1
  end

  # Reset signal (>80 us) + animation speed control
  sleep_ms 50

  offset = (offset + 1) % NUM_LEDS
end
