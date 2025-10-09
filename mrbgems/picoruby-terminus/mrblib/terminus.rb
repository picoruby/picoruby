module Terminus
  def self.draw(name, line, scale = 0)
    name = "_#{name}"
    # No scaling available. Just for compability with Shinonome font
    if block_given?
      yield(send(name, line.chomp))
    else
      send(name, line.chomp)
    end
  end
end

