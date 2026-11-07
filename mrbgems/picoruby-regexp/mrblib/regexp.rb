# Regexp and MatchData are implemented in C (src/mruby/regexp.c and
# src/mrubyc/regexp.c). This file holds the option constants and the
# String methods that take a block: mruby/c cannot call a block from
# C, so the loop that yields each match runs here, on byte offsets the
# C side reports. Everything without a block goes straight to C.

class Regexp
  IGNORECASE = 1
  EXTENDED   = 2
  MULTILINE  = 4

  # One Regexp that matches any of the patterns. A String matches
  # itself. Regexps must all carry the same options, since the engine
  # has no inline option groups. With no pattern the result never
  # matches.
  def self.union(*patterns)
    list = patterns #: Array[untyped]
    list = list[0] if list.size == 1 && list[0].is_a?(Array)
    return Regexp.new(".\\A") if list.empty?
    options = nil
    source = ""
    i = 0
    while i < list.size
      pat = list[i]
      source << "|" if 0 < i
      if pat.is_a?(Regexp)
        options ||= pat.options
        raise ArgumentError, "Regexp.union: the options differ" if options != pat.options
        source << "(?:" << pat.source << ")"
      elsif pat.is_a?(String)
        source << Regexp.escape(pat)
      else
        raise TypeError, "no implicit conversion of #{pat.class} into Regexp"
      end
      i += 1
    end
    Regexp.new(source, options || 0)
  end
end

class MatchData
  # [] takes a Range or a start and a length too, over to_a, as Array#[]
  # does; a group number or name stays with the C method.
  alias __aref_group []

  def [](*args)
    list = args #: Array[untyped]
    if list.size == 2
      part = to_a[list[0], list[1]] #: untyped
      return part
    end
    idx = list[0]
    if idx.is_a?(Range)
      part = to_a[idx] #: untyped
      return part
    end
    __aref_group(idx)
  end
end

class String
  # the String and nil forms of split stay with the VM's own split
  alias __split_str split

  # start_with? takes a Regexp too, which must match at the start; $~
  # follows the last Regexp tried, as in CRuby. A String prefix is
  # compared here, since a build may have no start_with? of its own.
  def start_with?(*prefixes)
    i = 0
    n = prefixes.size
    while i < n
      pre = prefixes[i] #: untyped
      if pre.is_a?(Regexp)
        md = pre.__bmatch(self, 0)
        md = nil if md && md.byteoffset(0)[0] != 0
        $~ = md
        return true if md
      elsif pre.is_a?(String)
        return true if byteslice(0, pre.bytesize) == pre
      else
        raise TypeError, "no implicit conversion of #{pre.class} into String"
      end
      i += 1
    end
    false
  end

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
    last_md = nil
    while pos <= len
      md = re.__bmatch(src, pos)
      break unless md
      matched = true
      last_md = md
      pair = md.byteoffset(0) #: [Integer, Integer]
      b = pair[0]
      e = pair[1]
      result << src.byteslice(last, b - last).to_s
      # the block reads this match through $~, $1, $& as in CRuby
      $~ = md
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
    $~ = last_md
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
    re = __regexp_for(pattern)
    return __scan(re) unless block
    # With a block, $~ is each match as it is yielded, as in CRuby. The
    # matches are read from a copy, which the block cannot change.
    src = self.dup
    len = src.bytesize
    pos = 0
    while pos <= len
      md = re.__bmatch(src, pos)
      break unless md
      pair = md.byteoffset(0) #: [Integer, Integer]
      b = pair[0]
      e = pair[1]
      $~ = md
      caps = md.captures
      block.call(caps.empty? ? md[0] : caps)
      if e == b
        break if len <= b
        pos = b + __charlen_at(src, b)
      else
        pos = e
      end
    end
    self
  end

  def split(*args)
    pattern = args[0] #: untyped
    return __split_str(*args) unless pattern.is_a?(Regexp)
    limit = (args[1] || 0) #: Integer
    __split(pattern, limit)
  end
end

# A Symbol matches as the String of its name, as in CRuby.
class Symbol
  def =~(pattern)
    to_s =~ pattern
  end

  def match(pattern, pos = 0)
    to_s.match(pattern, pos)
  end

  def match?(pattern, pos = 0)
    to_s.match?(pattern, pos)
  end
end
