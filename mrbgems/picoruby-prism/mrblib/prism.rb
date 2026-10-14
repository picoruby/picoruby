class Prism
end

class String
  def prettyprint
    lines = self.split("\n")
    i = 0
    while i < lines.size
      puts lines[i]
      i += 1
    end
    nil
  end
end
