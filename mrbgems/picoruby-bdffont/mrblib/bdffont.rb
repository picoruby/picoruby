module BDFFont
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
  end
end
