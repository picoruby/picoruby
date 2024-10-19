class String
  def rindex(needle)
    index = nil
    (self.size - 1).downto(0) do |i|
      if self[i] == needle
        index = i
        break
      end
    end
    p index
    index
  end

  def gsub(pattern, replacement)
    result = ''
    i = 0
    while i < self.size
      if self[i] == pattern[0]
        if self[i, pattern.size] == pattern
          result += replacement
          i += pattern.size
        else
          result += self[i].to_s
          i += 1
        end
      else
        result += self[i].to_s
        i += 1
      end
    end
    result
  end

end
