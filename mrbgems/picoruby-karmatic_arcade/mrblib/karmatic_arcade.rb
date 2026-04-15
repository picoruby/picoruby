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
    include BDFFont::Drawable

    def draw_karmatic_arcade(name, x, y, text, scale = 1)
      result = case name
               when "10" then KarmaticArcade._10(text, scale)
               else raise "Unsupported karmatic_arcade font: #{name}"
               end
      bdffont_draw_result(result, x, y)
    end
  end
end
