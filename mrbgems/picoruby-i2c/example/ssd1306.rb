require "i2c"
require "shinonome"

# SSD1306 OLED Display Driver for 128x64 pixels
# Usage: display = SSD1306.new; display.demo
class SSD1306
  DISPLAYOFF = 0xAE
  DISPLAYON = 0xAF
  SETCONTRAST = 0x81
  DISPLAYALLON_RESUME = 0xA4
  NORMALDISPLAY = 0xA6
  INVERTDISPLAY = 0xA7
  SETDISPLAYOFFSET = 0xD3
  SETCOMPINS = 0xDA
  SETVCOMDETECT = 0xDB
  SETDISPLAYCLOCKDIV = 0xD5
  SETPRECHARGE = 0xD9
  SETMULTIPLEX = 0xA8
  SETSTARTLINE = 0x40
  MEMORYMODE = 0x20
  COLUMNADDR = 0x21
  PAGEADDR = 0x22
  COMSCANDEC = 0xC8
  SEGREMAP = 0xA1
  CHARGEPUMP = 0x8D

  def initialize(address: 0x3C, width: 128, height: 64)
    @i2c = I2C.new(unit: :RP2040_I2C0, sda_pin: 8, scl_pin: 9, frequency: 400_000)
    @address = address
    @width = width
    @height = height
    @pages = height / 8
    @buffer = "\x00" * (@width * @pages)
    init_display
  end

  def init_display
    puts "Initializing SSD1306..."

    # Complete initialization sequence
    commands = [
      DISPLAYOFF,           # Turn off display
      SETDISPLAYCLOCKDIV, 0x80,  # Set display clock divide ratio/oscillator frequency
      SETMULTIPLEX, @height - 1, # Set multiplex ratio
      SETDISPLAYOFFSET, 0x00,    # Set display offset
      SETSTARTLINE | 0x00,       # Set start line address
      CHARGEPUMP, 0x14,          # Enable charge pump
      MEMORYMODE, 0x00,          # Horizontal addressing mode
      SEGREMAP | 0x01,           # Set segment re-map
      COMSCANDEC,                # Set COM scan direction
      SETCOMPINS, 0x12,          # Set COM pins hardware configuration
      SETCONTRAST, 0xCF,         # Set contrast
      SETPRECHARGE, 0xF1,        # Set pre-charge period
      SETVCOMDETECT, 0x40,       # Set VCOMH deselect level
      DISPLAYALLON_RESUME,       # Resume to RAM content display
      NORMALDISPLAY,             # Normal display (not inverted)
      DISPLAYON                  # Turn on display
    ]

    # Send all commands at once
    @i2c.write(@address, 0x00, commands)

    puts "SSD1306 initialized!"
    clear
  end

  # Clear the display
  def clear
    @buffer = "\x00" * (@width * @pages)
    update_display
  end

  # Fill screen with pattern
  def fill_screen(pattern = 0xFF)
    @buffer = pattern.chr * (@width * @pages)
    update_display
  end

  # Set a pixel at x, y
  def set_pixel(x, y, color = 1)
    return if x < 0 || x >= @width || y < 0 || y >= @height

    page = y / 8
    bit = y % 8
    index = page * @width + x

    if color == 1
      @buffer[index] = (@buffer[index].ord | (1 << bit)).chr
    else
      @buffer[index] = (@buffer[index].ord & ~(1 << bit)).chr
    end
  end

  # Draw a line from (x0,y0) to (x1,y1)
  def draw_line(x0, y0, x1, y1, color = 1)
    dx = (x1 - x0).abs
    dy = (y1 - y0).abs
    sx = x0 < x1 ? 1 : -1
    sy = y0 < y1 ? 1 : -1
    err = dx - dy

    while true
      set_pixel(x0, y0, color)
      break if x0 == x1 && y0 == y1

      e2 = 2 * err
      if e2 > -dy
        err -= dy
        x0 += sx
      end
      if e2 < dx
        err += dx
        y0 += sy
      end
    end
  end

  # Draw a rectangle
  def draw_rect(x, y, w, h, color = 1, fill = false)
    if fill
      0.upto(h - 1) do |i|
        0.upto(w - 1) do |j|
          set_pixel(x + j, y + i, color)
        end
      end
    else
      # Top and bottom edges
      0.upto(w - 1) do |i|
        set_pixel(x + i, y, color)
        set_pixel(x + i, y + h - 1, color)
      end
      # Left and right edges
      0.upto(h - 1) do |i|
        set_pixel(x, y + i, color)
        set_pixel(x + w - 1, y + i, color)
      end
    end
  end

  # Update the display with current buffer
  def update_display
    # Set addressing range
    @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
    @i2c.write(@address, 0x00, PAGEADDR, 0, @pages - 1)

    # Send entire buffer in one transaction
    @i2c.write(@address, 0x40, @buffer)
  end

  # Demo function showing various features
  def demo
    puts "Starting SSD1306 demo..."

    # Test 1: Fill screen
    puts "Test 1: Fill screen"
    fill_screen(0xFF)
    sleep 1
    GC.start

    # Test 2: Clear screen
    puts "Test 2: Clear screen"
    clear
    sleep 1
    GC.start

    # Test 3: Draw some pixels
    puts "Test 3: Drawing pixels"
    clear
    x = 0
    while x < @width
      y = 0
      while y < @height
        set_pixel(x, y, 1)
        y += 8
      end
      x += 8
    end
    update_display
    sleep 2
    GC.start

    # Test 4: Draw lines
    puts "Test 4: Drawing lines"
    clear
    draw_line(0, 0, @width-1, @height-1, 1)
    draw_line(@width-1, 0, 0, @height-1, 1)
    draw_line(@width/2, 0, @width/2, @height-1, 1)
    draw_line(0, @height/2, @width-1, @height/2, 1)
    update_display
    sleep 2
    GC.start

    # Test 5: Draw rectangles
    puts "Test 5: Drawing rectangles"
    clear
    draw_rect(10, 10, 30, 20, 1, false)  # Outline
    draw_rect(50, 20, 25, 15, 1, true)   # Filled
    draw_rect(85, 35, 35, 25, 1, false)  # Another outline
    update_display
    sleep 2
    GC.start

    # Test 6: Text display using shinonome font
    puts "Test 6: Text display with shinonome font"
    clear
    draw_text(0, 0, "いろはにほへとちりぬる", :maru12)
    draw_text(0, 16, "黒暗森林", :min16, 2)
    draw_text(0, 52, "Hello PicoRubyWorld!!!")
    update_display
    sleep 3
    GC.start

    puts "Demo completed!"
  end

  # Draw text using shinonome bitmap font
  def draw_text(x, y, text, fontname = :min12, scale = 1)
    Shinonome.draw(fontname, text, scale) do |height, total_width, widths, glyphs|
      glyph_x = x
      widths.each_with_index do |char_width, char_idx|
        # Draw each row of the character
        height.times do |row|
          char_width.times do |col|
            # Extract pixel from glyph bitmap
            if (glyphs[char_idx][row] >> (char_width - 1 - col)) & 1 == 1
              set_pixel(glyph_x + col, y + row, 1)
            end
          end
        end
        glyph_x += char_width
      end
    end
  end

end

# Usage examples
puts "SSD1306 OLED Display Example"
puts "Creating display instance..."

display = SSD1306.new

# Run the demo
display.demo

