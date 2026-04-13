require "i2c"
require "vram"
require "terminus"
begin
  require "shinonome"
rescue LoadError
end

class SSD1306
  include VRAM::Delegatable
  include Terminus::Drawable
  if Object.const_defined?(:Shinonome)
    include Shinonome::Drawable
  else
    def draw_shinonome(name, x, y, text, scale = 1)
      puts "Shinonome gem not available, skip text rendering"
    end
  end
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

  def erase(x, y, w, h)
    return if x < 0 || y < 0 || w <= 0 || h <= 0
    return if x >= @width || y >= @height
    # Clip the erase area to display bounds
    w = [@width - x, w].min
    h = [@height - y, h].min
    # @type var w: Integer
    # @type var h: Integer
    @vram.erase(x, y, w, h)
    nil
  end

  def fill_screen(pattern = 0xFF)
    @vram.fill(pattern)
    update_display
  end

  def draw_text(fontname, x, y, text, scale = 1)
    font, name = fontname.to_s.split("_")
    case font
    when 'shinonome'
      draw_shinonome(name.to_s, x, y, text, scale)
    when 'terminus'
      draw_terminus(name.to_s, x, y, text, scale)
    else
      raise "Unsupported font: #{font}"
    end
  end

  def update_display
    # Set addressing range
    @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
    @i2c.write(@address, 0x00, PAGEADDR, 0, @pages - 1)
    # Send all page data to display
    pages = @vram.pages
    i = 0
    while i < pages.size
      page = pages[i]
      @i2c.write(@address, 0x40, page[2])
      i += 1
    end
    pages.size
  end

  # Update only dirty pages for better performance
  def update_display_optimized
    dirty_pages = @vram.dirty_pages
    return 0 if dirty_pages.empty?
    i = 0
    while i < dirty_pages.size
      page = dirty_pages[i]
      # Set addressing range for this page only
      @i2c.write(@address, 0x00, COLUMNADDR, 0, @width - 1)
      @i2c.write(@address, 0x00, PAGEADDR, page[1], page[1])
      # Send page data
      @i2c.write(@address, 0x40, page[2])
      i += 1
    end
    dirty_pages.size
  end

end
