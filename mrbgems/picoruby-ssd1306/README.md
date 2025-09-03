# picoruby-ssd1306

SSD1306 OLED display driver for PicoRuby.

## Usage

```ruby
require 'ssd1306'

# Initialize I2C and display
i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 8, scl_pin: 9, frequency: 400_000)
display = SSD1306.new(i2c: i2c, w: 128, h: 64)

# Basic drawing
display.clear
display.set_pixel(64, 32, 1)
display.draw_line(0, 0, 127, 63, 1)
display.draw_rect(10, 10, 30, 20, 1, false)  # outline
display.draw_rect(50, 20, 25, 15, 1, true)   # filled
display.update_display

# Text with Shinonome fonts
display.draw_text(0, 0, "Hello World!", :min12)
display.draw_text(0, 16, "黒暗森林", :min16, 2)  # scale x2

# Bitmap drawing
icon = [0b01111110, 0b11100111, 0b11000011, 0b10111101]
display.draw_bitmap(x: 0, y: 0, w: 8, h: 4, data: icon)

# Byte array drawing
image_data = "\xFF\x00\xFF\x00"
display.draw_bytes(x: 0, y: 0, w: 16, h: 2, data: image_data)
```

## Methods

- `clear` - Clear screen
- `fill_screen(pattern)` - Fill with pattern
- `set_pixel(x, y, color)` - Set single pixel
- `draw_line(x0, y0, x1, y1, color)` - Draw line
- `draw_rect(x, y, w, h, color, fill)` - Draw rectangle
- `draw_bitmap(x:, y:, w:, h:, data:)` - Draw from integer array
- `draw_bytes(x:, y:, w:, h:, data:)` - Draw from byte string
- `draw_text(x, y, text, font, scale)` - Draw text (requires shinonome gem)
- `update_display` - Update entire display
- `update_display_optimized` - Update only changed areas

## Supported Displays

- 128x64 OLED (default)
- Custom sizes via `w:` and `h:` parameters
- I2C address 0x3C (default) or custom via `address:` parameter
