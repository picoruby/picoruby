module Terminus
  def self.draw(fontname, line, scale = 0, &block)
    # No scaling available. Just for compability with Shinonome font
    if block_given?
      yield(send(fontname, line.chomp))
    else
      send(fontname, line.chomp)
    end
  end
end

