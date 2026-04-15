module Terminus
  def self.draw(name, line, scale = 1)
    name = "_#{name}"
    if block_given?
      yield(send(name, line.chomp, scale))
    else
      send(name, line.chomp, scale)
    end
  end

  module Drawable
    include BDFFont::Drawable

    def draw_terminus(name, x, y, text, scale = 1)
      result = case name
               when "6x12"  then Terminus._6x12(text, scale)
               when "8x16"  then Terminus._8x16(text, scale)
               when "12x24" then Terminus._12x24(text, scale)
               when "16x32" then Terminus._16x32(text, scale)
               else raise "Unsupported terminus font: #{name}"
               end
      bdffont_draw_result(result, x, y)
    end
  end
end
