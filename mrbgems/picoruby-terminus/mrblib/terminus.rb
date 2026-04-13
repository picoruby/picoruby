module Terminus
  def self.draw(name, line, scale = 1)
    name = "_#{name}"
    if block_given?
      yield(send(name, line.chomp, scale))
    else
      send(name, line.chomp, scale)
    end
  end
end

