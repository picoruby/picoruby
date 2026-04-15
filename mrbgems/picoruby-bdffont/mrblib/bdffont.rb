module BDFFont
  def self.setup(klass)
    begin
      require "terminus"
      klass.include Terminus::Drawable
    rescue LoadError
    end
    begin
      require "karmatic_arcade"
      klass.include KarmaticArcade::Drawable
    rescue LoadError
    end
    begin
      require "shinonome"
      klass.include Shinonome::Drawable
    rescue LoadError
    end
  end

  module Drawable
    def bdffont_draw_result(result, x, y)
      height = result[0]
      widths = result[2]
      glyphs = result[3]
      glyph_x = x
      i = 0
      while i < widths.size
        draw_bitmap(x: glyph_x, y: y, w: widths[i], h: height, data: glyphs[i])
        glyph_x += widths[i]
        i += 1
      end
      nil
    end

    def draw_text(fontname, x, y, text, scale = 1)
      font, name = fontname.to_s.split("_")
      case font
      when "terminus"
        if Object.const_defined?(:Terminus)
          draw_terminus(name.to_s, x, y, text, scale)
        else
          puts "Terminus font is not available"
        end
      when "shinonome"
        if Object.const_defined?(:Shinonome)
          draw_shinonome(name.to_s, x, y, text, scale)
        else
          puts "Shinonome font is not available"
        end
      when 'karmatic-arcade'
        if Object.const_defined?(:KarmaticArcade)
          draw_karmatic_arcade(name.to_s, x, y, text, scale)
        else
          puts "Karmatic Arcade font is not available"
        end
      else
        raise "Unsupported font: #{font}"
      end
    end
  end
end
