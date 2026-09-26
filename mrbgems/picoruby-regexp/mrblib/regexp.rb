# Regexp and MatchData are implemented in C (src/mruby/regexp.c and
# src/mrubyc/regexp.c). This file holds the option constants and the
# String methods that take a block: mruby/c cannot call a block from
# C, so the loop that yields each match runs here, on byte offsets the
# C side reports. Everything without a block goes straight to C.

class Regexp
  IGNORECASE = 1
  EXTENDED   = 2
  MULTILINE  = 4
end

class String
  # the String and nil forms of split stay with the VM's own split
  alias __split_str split

  # The Regexp a pattern argument stands for. A String matches itself.
  def __regexp_for(pattern)
    return pattern if pattern.is_a?(Regexp)
    return Regexp.new(Regexp.escape(pattern)) if pattern.is_a?(String)
    raise TypeError, "wrong argument type #{pattern.class} (expected Regexp)"
  end

  # sub / gsub with a String replacement, a Hash or a block. Returns
  # the new String, or nil when nothing matched.
  def __sub_common(pattern, args, block, global)
    re = __regexp_for(pattern)
    if args.size == 1
      repl = args[0]
      if repl.is_a?(Hash)
        __sub_each(re, global) { |m| repl[m].to_s }
      elsif repl.is_a?(String)
        __sub(re, repl, global)
      else
        raise TypeError, "no implicit conversion of #{repl.class} into String"
      end
    elsif args.empty? && block
      __sub_each(re, global) { |m| block.call(m).to_s }
    else
      raise ArgumentError, "wrong number of arguments (given #{args.size + 1}, expected 2)"
    end
  end

  # Replace each match (the first one unless global) with what the
  # block returns for the matched text. nil when nothing matched. The
  # receiver must not change while the block runs.
  def __sub_each(re, global)
    src = self.dup
    len = src.bytesize
    result = ""
    pos = 0
    last = 0
    matched = false
    while pos <= len
      md = re.__bmatch(src, pos)
      break unless md
      matched = true
      pair = md.byteoffset(0) #: [Integer, Integer]
      b = pair[0]
      e = pair[1]
      result << src.byteslice(last, b - last).to_s
      result << yield(md.to_s)
      raise RuntimeError, "string modified" unless self == src
      if e == b
        if len <= b
          last = len
          break
        end
        # an empty match: keep one character and search behind it
        w = __charlen_at(src, b)
        result << src.byteslice(b, w).to_s
        pos = b + w
        last = pos
      else
        pos = e
        last = e
      end
      break unless global
    end
    return nil unless matched
    result << src.byteslice(last, len - last).to_s
    result
  end

  # bytes of the UTF-8 character at byte offset b of str
  def __charlen_at(str, b)
    c = str.getbyte(b) || 0
    return 1 if c < 0xC0
    return 2 if c < 0xE0
    return 3 if c < 0xF0
    4
  end

  def sub(pattern, *args, &block)
    __sub_common(pattern, args, block, false) || dup
  end

  def gsub(pattern, *args, &block)
    __sub_common(pattern, args, block, true) || dup
  end

  def sub!(pattern, *args, &block)
    result = __sub_common(pattern, args, block, false)
    return nil unless result
    replace(result)
  end

  def gsub!(pattern, *args, &block)
    result = __sub_common(pattern, args, block, true)
    return nil unless result
    replace(result)
  end

  # Every match: the matched text, or the Array of groups when the
  # pattern has any. With a block each match is yielded instead.
  def scan(pattern, &block)
    matches = __scan(__regexp_for(pattern))
    return matches unless block
    matches.each { |m| block.call(m) }
    self
  end

  def split(*args)
    pattern = args[0] #: untyped
    return __split_str(*args) unless pattern.is_a?(Regexp)
    limit = (args[1] || 0) #: Integer
    __split(pattern, limit)
  end
end
