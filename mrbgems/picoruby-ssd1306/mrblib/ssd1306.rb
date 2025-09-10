require "i2c"
require "vram"
require "terminus"

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

  def initialize(i2c:, address: 0x3C, w: 128, h: 64)
    @i2c = i2c
    @address = address
    @width = w
    @height = h
    @pages = h / 8
    # Create VRAM with single column, multiple rows (one per page)
    @vram = VRAM.new(w: @width, h: @height, cols: 1, rows: @pages)
    @vram.name = "SSD1306"

    init_display
  end

  private def init_display
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
    clear
  end

  def clear
    # Fill all pages with 0 (clear)
    @vram.fill(0x00)
    update_display
  end

  def fill_screen(pattern = 0xFF)
    @vram.fill(pattern)
    update_display
  end

  def set_pixel(x, y, color = 1)
    return if x < 0 || x >= @width || y < 0 || y >= @height
    @vram.set_pixel(x, y, color)
  end

  def draw_line(x0, y0, x1, y1, color = 1)
    @vram.draw_line(x0, y0, x1, y1, color)
    nil
  end

  def draw_rect(x, y, w, h, color = 1, fill = false)
    if fill
      @vram.draw_rect(x, y, w, h, color)
    else
      # Draw outline using lines
      draw_line(x, y, x + w - 1, y, color)                   # Top
      draw_line(x, y + h - 1, x + w - 1, y + h - 1, color)   # Bottom
      draw_line(x, y, x, y + h - 1, color)                   # Left
      draw_line(x + w - 1, y, x + w - 1, y + h - 1, color)   # Right
    end
    nil
  end

  def draw_bitmap(x:, y:, w:, h:, data:)
    @vram.draw_bitmap(x: x, y: y, w: w, h: h, data: data)
    nil
  end

  def draw_bytes(x:, y:, w:, h:, data:)
    @vram.draw_bytes(x: x, y: y, w: w, h: h, data: data)
    nil
  end

  def draw_text(fontname_size, x, y, text, scale = 1)
    fontname, size = fontname_size.to_s.split("_")
    case fontname
    when 'shinonome'
      draw_shinonome(size.to_s, x, y, text, scale)
    when 'terminus'
      draw_terminus("_#{size}", x, y, text)
    else
      raise "Unsupported font: #{fontname}"
    end
  end

  def draw_terminus(size, x, y, text)
    Terminus.draw(size, text) do |height, total_width, widths, glyphs|
      glyph_x = x
      widths.each_with_index do |char_width, char_idx|
        # Use optimized draw_bitmap for each character
        char_data = glyphs[char_idx]
        draw_bitmap(x: glyph_x, y: y, w: char_width, h: height, data: char_data)
        glyph_x += char_width
      end
    end
    nil
  end

  begin
    require "shinonome"
    shinonome_available = true
    def draw_shinonome(size, x, y, text, scale = 1)
      Shinonome.draw(size, text, scale) do |height, total_width, widths, glyphs|
        glyph_x = x
        widths.each_with_index do |char_width, char_idx|
          # Use optimized draw_bitmap for each character
          char_data = glyphs[char_idx]
          draw_bitmap(x: glyph_x, y: y, w: char_width, h: height, data: char_data)
          glyph_x += char_width
        end
      end
      nil
    end
  rescue LoadError
    def draw_shinonome(fontname, x, y, text, scale = 1)
      puts "Shinonome gem not available, skip text rendering"
    end
  end

  def update_display
    # Set addressing range
    @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
    @i2c.write(@address, 0x00, PAGEADDR, 0, @pages - 1)
    # Send all page data to display
    @vram.pages.each do |col, row, data|
      @i2c.write(@address, 0x40, data)
    end
    @vram.pages.size
  end

  # Update only dirty pages for better performance
  def update_display_optimized
    dirty_pages = @vram.dirty_pages
    return 0 if dirty_pages.empty?
    dirty_pages.each do |col, row, data|
      # Set addressing range for this page only
      @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
      @i2c.write(@address, 0x00, PAGEADDR, row, row)
      # Send page data
      @i2c.write(@address, 0x40, data)
    end
    dirty_pages.size
  end

end
