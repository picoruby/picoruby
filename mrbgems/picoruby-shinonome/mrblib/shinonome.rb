module Shinonome
  def self.draw(fontname, line, scale = 1, &block)
    if block_given?
      yield(send(fontname, line.chomp, scale))
    else
      send(fontname, line.chomp, scale)
    end
  end
end
