class VRAM
  attr_accessor :name
end

module VRAM::Delegatable
  def set_pixel(x, y, color = 1)
    return if x < 0 || x >= @width || y < 0 || y >= @height
    @vram.set_pixel(x, y, color)
  end

  def draw_bitmap(x:, y:, w:, h:, data:)
    @vram.draw_bitmap(x: x, y: y, w: w, h: h, data: data)
    nil
  end

  def draw_bytes(x:, y:, w:, h:, data:)
    @vram.draw_bytes(x: x, y: y, w: w, h: h, data: data)
    nil
  end

  def draw_line(x0, y0, x1, y1, color = 1)
    @vram.draw_line(x0, y0, x1, y1, color)
    nil
  end

  def draw_rect(x, y, w, h, color = 1, fill = false)
    if fill
      @vram.draw_rect(x, y, w, h, color)
    else
      draw_line(x,         y,         x + w - 1, y,         color)
      draw_line(x,         y + h - 1, x + w - 1, y + h - 1, color)
      draw_line(x,         y,         x,         y + h - 1, color)
      draw_line(x + w - 1, y,         x + w - 1, y + h - 1, color)
    end
    nil
  end
end
