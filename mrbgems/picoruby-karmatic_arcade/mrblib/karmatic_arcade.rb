module KarmaticArcade
  def self.draw(name, line, scale = 1)
    name = "_#{name}"
    if block_given?
      yield(send(name, line.chomp, scale))
    else
      send(name, line.chomp, scale)
    end
  end

  module Drawable
    def draw_karmatic_arcade(name, x, y, text, scale = 1)
      result = case name
               when "20" then KarmaticArcade._20(text, scale)
               else raise "Unsupported karmatic_arcade font: #{name}"
               end
      height = result[0]
      widths = result[2]
      glyphs = result[3]
      glyph_x = x
      i = 0
      while i < widths.size
        char_width = widths[i]
        char_data = glyphs[i]
        draw_bitmap(x: glyph_x, y: y, w: char_width, h: height, data: char_data)
        glyph_x += char_width
        i += 1
      end
      nil
    end
  end
end
