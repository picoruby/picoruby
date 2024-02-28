class Prism
end

class String
  def prettyprint
    self.split("\n").each{|line| puts line}
    nil
  end
end
