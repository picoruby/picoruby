require "i2c"
require "vram"
require "shinonome"

# SSD1306 OLED Display Driver using VRAM class for 128x64 pixels
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

    # Create VRAM with single column, multiple rows (one per page)
    @vram = VRAM.new(w: @width, h: @height, cols: 1, rows: @pages)
    @vram.name = "SSD1306"

    init_display
  end

  def init_display
    puts "Initializing SSD1306 with VRAM..."

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

    puts "SSD1306 with VRAM initialized!"
    clear
  end

  # Clear the display using VRAM fill functionality
  def clear
    # Fill all pages with 0 (clear)
    @vram.fill(0x00)
    update_display
  end

  # Fill screen with pattern using VRAM
  def fill_screen(pattern = 0xFF)
    @vram.fill(pattern)
    update_display
  end

  # Set a pixel using VRAM set_pixel method
  def set_pixel(x, y, color = 1)
    return if x < 0 || x >= @width || y < 0 || y >= @height
    @vram.set_pixel(x, y, color)
  end

  # Draw a line using VRAM draw_line method
  def draw_line(x0, y0, x1, y1, color = 1)
    @vram.draw_line(x0, y0, x1, y1, color)
  end

  # Draw a rectangle using VRAM methods
  def draw_rect(x, y, w, h, color = 1, fill = false)
    if fill
      @vram.draw_rect(x, y, w, h, color)
    else
      # Draw outline using lines
      draw_line(x, y, x + w - 1, y, color)                    # Top
      draw_line(x, y + h - 1, x + w - 1, y + h - 1, color)   # Bottom
      draw_line(x, y, x, y + h - 1, color)                    # Left
      draw_line(x + w - 1, y, x + w - 1, y + h - 1, color)   # Right
    end
  end

  # Update the display with current VRAM buffer
  def update_display
    # Set addressing range
    @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
    @i2c.write(@address, 0x00, PAGEADDR, 0, @pages - 1)

    # Send all page data to display
    @vram.pages.each do |col, row, data|
      @i2c.write(@address, 0x40, data)
    end
  end

  # Update only dirty pages for better performance
  def update_display_optimized
    dirty_pages = @vram.dirty_pages
    return if dirty_pages.empty?

    dirty_pages.each do |col, row, data|
      # Set addressing range for this page only
      @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
      @i2c.write(@address, 0x00, PAGEADDR, row, row)

      # Send page data
      @i2c.write(@address, 0x40, data)
    end
  end

  # Demo function showing various features using VRAM
  def demo
    puts "Starting SSD1306 VRAM demo..."

    # Test 1: Fill screen
    puts "Test 1: Fill screen using VRAM"
    fill_screen(0xFF)
    sleep 1
    GC.start

    # Test 2: Clear screen
    puts "Test 2: Clear screen using VRAM"
    clear
    sleep 1
    GC.start

    # Test 3: Draw some pixels
    puts "Test 3: Drawing pixels using VRAM"
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

    # Test 4: Draw lines using VRAM
    puts "Test 4: Drawing lines using VRAM"
    clear
    draw_line(0, 0, @width-1, @height-1, 1)
    draw_line(@width-1, 0, 0, @height-1, 1)
    draw_line(@width/2, 0, @width/2, @height-1, 1)
    draw_line(0, @height/2, @width-1, @height/2, 1)
    update_display
    sleep 2
    GC.start

    # Test 5: Draw rectangles using VRAM
    puts "Test 5: Drawing rectangles using VRAM"
    clear
    draw_rect(10, 10, 30, 20, 1, false)  # Outline
    draw_rect(50, 20, 25, 15, 1, true)   # Filled
    draw_rect(85, 35, 35, 25, 1, false)  # Another outline
    update_display
    sleep 2
    GC.start

    # Test 6: Performance test with optimized updates
    puts "Test 6: Performance test with optimized dirty page updates"
    clear
    10.times do |i|
      # Only update a small area
      draw_rect(i * 10, i * 5, 15, 10, 1, true)
      update_display_optimized  # Only update dirty pages
      sleep 0.1
    end
    sleep 2
    GC.start

    # Test 7: Text display using shinonome font with VRAM
    puts "Test 7: Text display with shinonome font using VRAM"
    clear
    draw_text(0, 0, "Hello PicoRubyWorld!!")
    update_display_optimized
    draw_text(0, 16, "黒暗森林", :min16, 2)
    update_display_optimized
    draw_text(0, 52, "いろはにほへとちりぬる", :maru12)
    update_display_optimized
    GC.start

    puts "VRAM demo completed!"
  end

  # Draw text using shinonome bitmap font with VRAM
  def draw_text(x, y, text, fontname = :min12, scale = 1)
    Shinonome.draw(fontname, text, scale) do |height, total_width, widths, glyphs|
      glyph_x = x
      widths.each_with_index do |char_width, char_idx|
        # Draw each row of the character using VRAM
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
puts "SSD1306 OLED Display Example with VRAM"
puts "Creating display instance with VRAM..."

display = SSD1306.new

# Run the demo
display.demo
