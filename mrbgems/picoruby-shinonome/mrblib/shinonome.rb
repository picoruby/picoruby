module Shinonome
  def self.draw(name, line, scale = 1)
    if block_given?
      yield(send(name, line.chomp, scale))
    else
      send(name, line.chomp, scale)
    end
  end

  module Drawable
    def draw_shinonome(name, x, y, text, scale = 1)
      # Call C method directly to avoid send -> mrb_funcall -> mrb_vm_exec recursion
      result = case name
               when "test12" then Shinonome.test12(text, scale)
               when "test16" then Shinonome.test16(text, scale)
               when "maru12" then Shinonome.maru12(text, scale)
               when "go12"   then Shinonome.go12(text, scale)
               when "min12"  then Shinonome.min12(text, scale)
               when "go16"   then Shinonome.go16(text, scale)
               when "min16"  then Shinonome.min16(text, scale)
               else raise "Unsupported shinonome font: #{name}"
               end
      if result.nil?
        return # maybe test12 or test16
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
