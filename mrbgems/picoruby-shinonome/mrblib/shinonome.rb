module Shinonome
  def self.draw(name, line, scale = 1)
    if block_given?
      yield(send(name, line.chomp, scale))
    else
      send(name, line.chomp, scale)
    end
  end

  module Drawable
    include BDFFont::Drawable

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
      bdffont_draw_result(result, x, y)
    end
  end
end
