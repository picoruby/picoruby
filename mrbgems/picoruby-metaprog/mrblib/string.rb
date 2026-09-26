# String#gsub, sub, scan and split with a Regexp come from picoruby-regexp
require "regexp"

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
end
