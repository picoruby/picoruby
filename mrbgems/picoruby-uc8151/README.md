# picoruby-uc8151

UC8151 e-ink display driver for PicoRuby.

## Usage

```ruby
require 'uc8151'

# Initialize SPI and display
spi = SPI.new(
  unit:      :RP2040_SPI0,
  frequency: 2_000_000,
  sck_pin:   18,
  copi_pin:  19
)
display = UC8151.new(
  spi:      spi,
  cs_pin:   17,
  dc_pin:   20,
  rst_pin:  21,
  busy_pin: 26
)

# Basic drawing
display.fill(0)                             # fill with black
display.fill(1)                             # fill with white (paper color)
display.set_pixel(148, 64, 1)
display.draw_line(0, 0, 295, 127, 1)
display.draw_rect(10, 10, 60, 40, 1, false) # outline
display.draw_rect(80, 10, 60, 40, 1, true)  # filled
display.update

# Text with Shinonome fonts
display.draw_text(:shinonome_min16, 0, 0,  "Hello World!")
display.draw_text(:shinonome_min12, 0, 20, "こんにちは", 2)  # scale x2

# Text with Terminus fonts
display.draw_text(:terminus_12x24, 0, 80, "@hasumikin")

# Bitmap drawing
icon = [0b01111110, 0b11100111, 0b11000011, 0b10111101]
display.draw_bitmap(x: 0, y: 0, w: 8, h: 4, data: icon)

# Byte array drawing
image_data = "\xFF\x00\xFF\x00"
display.draw_bytes(x: 0, y: 0, w: 16, h: 2, data: image_data)
```

## Methods

- `fill(color)` - Fill screen with color (`0` = black, `1` = white)
- `set_pixel(x, y, color)` - Set single pixel
- `draw_line(x0, y0, x1, y1, color)` - Draw line
- `draw_rect(x, y, w, h, color, fill)` - Draw rectangle
- `draw_bitmap(x:, y:, w:, h:, data:)` - Draw from integer array
- `draw_bytes(x:, y:, w:, h:, data:)` - Draw from byte string
- `draw_text(font, x, y, text, scale)` - Draw text (supports Shinonome and Terminus fonts)
- `update` - Send framebuffer to display and trigger refresh

## Supported Display

- 296×128 e-ink panel (default)
- Custom sizes via `w:` and `h:` parameters
- Communicates over SPI with CS, DC, RST, and BUSY control pins

## Notes

- The BUSY pin is active-low: the driver waits until the panel signals idle before proceeding.
- `update` powers off the panel (POF) after refresh to reduce power consumption.
- On first boot, `initialize` performs a `deep_clean` to clear ghost images from previous sessions.
- `draw_text` with Shinonome fonts requires the optional `picoruby-shinonome` gem.
