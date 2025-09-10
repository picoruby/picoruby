module Terminus
  def self.draw(fontname, line, &block)
    if block_given?
      yield(send(fontname, line.chomp))
    else
      send(fontname, line.chomp)
    end
  end
end

