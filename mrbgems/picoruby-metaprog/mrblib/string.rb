class String
  def rindex(needle) # steep:ignore MethodArityMismatch
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

  # PicoRuby's simplified gsub only accepts String args, not Regexp or block
  def gsub(pattern, replacement) # steep:ignore MethodParameterMismatch
    # @type var pattern: String
    # @type var replacement: String
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
