#
# YAM library for PicoRuby
# This is a simple YAML library for PicoRuby.
# It is designed to be small and simple, not to be fast or complete.
#
# Author: Hitoshi HASUMI
# License: MIT
#

module YAML
  def self.load(yaml_string)
    parse(yaml_string)
  end

  def self.load_file(file_path)
    File.open(file_path, "r") do |file|
      parse(file.read)
    end
  end

  def self.dump(ruby_object)
    serialize(ruby_object)
  end

#  private

  def self.parse(yaml_string)
    # &name: the node each anchor names, for *name and << to refer to
    anchors = {} #: Hash[String, untyped]
    lines = yaml_string.split("\n")
    root = {} #: Hash[untyped, untyped]
    stack = [root] #: Array[untyped]
    indent_stack = [-1]
    prev_indent = -1

    li = 0
    while li < lines.size
      line = lines[li]
      li += 1
      line_strip = line.strip
      next if line_strip.empty? || line_strip.start_with?('#')

      indent = count_leading_spaces(line)
      is_list_item = line_strip.start_with?('-')

      if is_list_item
        key, value = parse_list_item(line_strip)
      else
        key, value = split_key_value(line_strip)
      end

      while indent <= indent_stack.last && 1 < stack.size
        indent_stack.pop
        stack.pop
      end

      current_object = stack.last

      if is_list_item
        if !current_object.is_a?(Array)
          # The container was opened as an empty Hash before its first
          # line showed it is a list. Replace it where its parent holds it.
          new_array = [] #: Array[untyped]
          if 1 < stack.size
            parent = stack[-2]
            if parent.is_a?(Hash)
              last_key = parent.keys.last
              parent[last_key] = new_array
            elsif parent.is_a?(Array)
              parent[parent.size - 1] = new_array
            end
          end
          replace_anchor(anchors, stack[-1], new_array)
          stack[-1] = new_array
          current_object = new_array
        end
        anchor = anchor_only(value)
        if value.nil? || value.empty? || anchor
          new_item = {} #: Hash[untyped, untyped]
          current_object << new_item
          anchors[anchor] = new_item if anchor
          stack.push(new_item)
          indent_stack.push(indent)
        elsif mapping_item?(value)
          # `- key: value` opens a Hash item. Its other keys follow at
          # the column of this key, so the item stays on the stack at the
          # dash's indent and a key of a nested value at its own column.
          item_key, item_value = split_key_value(value)
          new_item = {} #: Hash[untyped, untyped]
          current_object << new_item
          stack.push(new_item)
          indent_stack.push(indent)
          anchor = anchor_only(item_value)
          if item_value.nil? || item_value.empty? || anchor
            nested = {} #: Hash[untyped, untyped]
            new_item[item_key] = nested
            anchors[anchor] = nested if anchor
            stack.push(nested)
            indent_stack.push(indent + line_strip.length - value.length)
          else
            assign(new_item, item_key, parse_value(item_value, anchors))
          end
        else
          current_object << parse_value(value, anchors)
        end
      else
        anchor = anchor_only(value)
        if value.nil? || value.empty? || anchor
          new_item = {} #: Hash[untyped, untyped]
          current_object[key] = new_item
          anchors[anchor] = new_item if anchor
          stack.push(new_item)
          indent_stack.push(indent)
        else
          assign(current_object, key, parse_value(value, anchors))
        end
      end

      prev_indent = indent
    end

    # A document whose first line is a list item replaced the root Hash.
    stack[0]
  end

  # "&name" with nothing after it: the anchor of the block collection on
  # the lines below. nil for anything else.
  def self.anchor_only(value)
    return nil unless value && value.getbyte(0) == 38 # &
    value = strip_comment(value)
    return nil if value.include?(" ")
    value.byteslice(1, value.bytesize - 1).to_s
  end

  # A Hash opened for a block collection turned out to be a list; an
  # anchor on it names the list.
  def self.replace_anchor(anchors, old_object, new_object)
    # object_id, as mruby/c has no equal?: the very object, not an equal one
    old_id = old_object.object_id
    names = anchors.keys
    names_size = names.size
    i = 0
    while i < names_size
      name = names[i]
      anchors[name] = new_object if anchors[name].object_id == old_id
      i += 1
    end
  end

  # hash[key] = value, except that a << key merges the Hash it is given,
  # or each Hash of a list of them, the first one winning, into hash
  def self.assign(hash, key, value)
    if key == "<<" && value.is_a?(Hash)
      hash.merge!(value)
    elsif key == "<<" && value.is_a?(Array)
      i = value.size - 1
      while 0 <= i
        item = value[i]
        hash.merge!(item) if item.is_a?(Hash)
        i -= 1
      end
    else
      hash[key] = value
    end
  end

  # A scalar, a quoted string ("..." with escapes, '...' with ''), a flow
  # sequence or mapping, ~ (nil) or a :symbol, as Psych reads them; a
  # # comment after a plain value is not part of it. `anchors` holds the
  # nodes *name and &name refer to, while a document is parsed.
  def self.parse_value(value, anchors = nil)
    value = value.strip
    first = value.getbyte(0)
    if first == 42 && anchors # *alias: the very node the anchor names
      name = strip_comment(value)
      name = name.byteslice(1, name.bytesize - 1).to_s
      unless anchors.has_key?(name)
        raise ArgumentError, "An alias referenced an unknown anchor: #{name}"
      end
      return anchors[name]
    elsif first == 38 && anchors # &anchor value
      parts = value.split(" ", 2)
      name = parts[0].byteslice(1, parts[0].bytesize - 1).to_s
      rest = parts[1]
      node = rest.nil? || rest.empty? ? nil : parse_value(rest, anchors)
      anchors[name] = node
      return node
    end
    if first == 34 # "
      r = parse_double_quoted(value)
      return r if r
    elsif first == 39 # '
      r = parse_single_quoted(value)
      return r if r
    elsif first == 91 # [
      r = parse_flow(value, 93, anchors)
      return r if r
    elsif first == 123 # {
      r = parse_flow(value, 125, anchors)
      return r if r
    end
    value = strip_comment(value)
    return nil if value == "~"
    # :name is a Symbol; :8080 and ::1 stay Strings
    second = value.getbyte(1)
    if first == 58 && second && !value.include?(" ") &&
       (second == 95 || (65 <= second && second <= 90) || (97 <= second && second <= 122))
      return value.byteslice(1, value.bytesize - 1).to_s.to_sym
    end
    if is_integer?(value)
      value.to_i
    elsif is_float?(value)
      value.to_f
    elsif is_boolean?(value)
      value.downcase == 'true'
    elsif is_null?(value)
      nil
    else
      value
    end
  end

  def self.serialize(object, indent = 0)
    case object
    when Hash
      yaml = ""
      keys = object.keys
      ki = 0
      while ki < keys.size
        key = keys[ki]
        value = object[key]
        key = quote(key) if key.is_a?(String) && !plain_key?(key)
        yaml << "#{' ' * indent}#{key}:"
        if value.nil?
          yaml << " null\n"
        elsif value.is_a?(Hash) || value.is_a?(Array)
          yaml << "\n" + serialize(value, indent + 2)
        else
          yaml << " #{serialize(value)}\n"
        end
        ki += 1
      end
      yaml
    when Array
      yaml = ""
      # @type var object: Array
      ai = 0
      while ai < object.size
        item = object[ai]
        # @type var item: Hash[untyped, untyped] | Array[untyped]
        yaml << "#{' ' * indent}- "
        if item.nil?
          yaml << "null\n"
        elsif item.is_a?(Hash) || item.is_a?(Array)
          yaml << "\n" + serialize(item, indent + 2)
        else
          yaml << "#{serialize(item)}\n"
        end
        ai += 1
      end
      yaml
    when String
      plain?(object) ? object : quote(object)
    else
      object.to_s
    end
  end

  # The first characters that make a plain value something else
  INDICATORS = "[]{}*&:~\"'#!|>%@`,"

  # whether the String s, written as it is, loads back as itself
  def self.plain?(s)
    return false if s.empty? || s != s.strip
    return false if INDICATORS.include?(s.byteslice(0, 1).to_s)
    return false if (s.start_with?("- ") || s.start_with?("? ") || s == "-" || s == "?")
    return false if s.include?(" #") || s.include?(": ") || s.end_with?(":")
    return false if other_value_in_psych?(s)
    i = 0
    n = s.bytesize
    while i < n
      return false if s.getbyte(i).to_i < 32
      i += 1
    end
    parse_value(s) == s
  end

  # whether Psych (YAML 1.1) may read s as a value other than a String, so
  # that a dumped file loads the same on a host. A number, a date or a time
  # starts with a digit, a sign or a dot, and yes, no, on and off are
  # booleans there. A String quoted without need still loads as itself.
  def self.other_value_in_psych?(s)
    first = s.getbyte(0).to_i
    return true if (48 <= first && first <= 57) || first == 43 || first == 45 || first == 46
    word = s.downcase
    word == "yes" || word == "no" || word == "on" || word == "off"
  end

  # a key is read up to its first : and a line starting with - is a list item
  def self.plain_key?(s)
    plain?(s) && !s.include?(":") && !s.start_with?("-")
  end

  HEX_DIGITS = "0123456789abcdef"

  # s as a "..." scalar that parse_double_quoted reads back
  def self.quote(s)
    out = '"'
    i = 0
    n = s.bytesize
    while i < n
      c = s.getbyte(i).to_i
      if c == 34 then out << '\\"'
      elsif c == 92 then out << '\\\\'
      elsif c == 10 then out << '\\n'
      elsif c == 9 then out << '\\t'
      elsif c == 13 then out << '\\r'
      elsif c < 32 then out << '\\x' << HEX_DIGITS[c >> 4].to_s << HEX_DIGITS[c & 15].to_s
      else out << s.byteslice(i, 1).to_s
      end
      i += 1
    end
    out << '"'
  end

  def self.count_leading_spaces(string)
    string.length - string.lstrip.length
  end

  def self.split_key_value(string)
    if quoted?(string)
      e = quote_end(string)
      if e && string.getbyte(e + 1) == 58 # a quoted key may hold a :
        key = parse_value(string.byteslice(0, e + 1).to_s).to_s
        return [key, string.byteslice(e + 2, string.bytesize - e - 2).to_s.strip]
      end
    end
    parts = string.split(':', 2)
    key = parts[0].strip
    key = parse_value(key).to_s if quoted?(key)
    [key, parts[1]&.strip]
  end

  def self.quoted?(string)
    first = string.getbyte(0)
    first == 34 || first == 39
  end

  # the index of the quote that closes the quoted scalar string starts
  # with; nil when none does
  def self.quote_end(string)
    q = string.getbyte(0)
    i = 1
    n = string.bytesize
    while i < n
      c = string.getbyte(i)
      if c == 92 && q == 34 # an escape in "..."
        i += 2
      elsif c == q
        return i unless q == 39 && string.getbyte(i + 1) == 39 # '' in '...'
        i += 2
      else
        i += 1
      end
    end
    nil
  end

  # whether the text of a list item is a `key: value` or `key:` mapping
  # item; a quoted key is one when a : follows its closing quote
  def self.mapping_item?(value)
    if quoted?(value)
      e = quote_end(value)
      return false unless e && value.getbyte(e + 1) == 58 # :
      c = value.getbyte(e + 2)
      c.nil? || c == 32
    else
      value.include?(': ') || value.end_with?(':')
    end
  end

  # plain text up to a " #" that starts a comment
  def self.strip_comment(value)
    i = 1
    n = value.bytesize
    while i < n
      if value.getbyte(i) == 35 && value.getbyte(i - 1) == 32 # " #"
        return value.byteslice(0, i - 1).to_s.strip
      end
      i += 1
    end
    value
  end

  # "..." with its escapes; nil when no quote closes it
  def self.parse_double_quoted(value)
    out = ""
    i = 1
    n = value.bytesize
    while i < n
      c = value.getbyte(i)
      return out if c == 34
      if c == 92 # backslash
        i += 1
        e = value.getbyte(i)
        if e == 110 then out << "\n"
        elsif e == 116 then out << "\t"
        elsif e == 114 then out << "\r"
        elsif e == 48 then out << "\0"
        elsif e == 101 then out << "\e"
        elsif e == 97 then out << "\a"
        elsif e == 98 then out << "\b"
        elsif e == 102 then out << "\f"
        elsif e == 118 then out << "\v"
        elsif e == 120 || e == 117 || e == 85 # \xXX \uXXXX \UXXXXXXXX
          len = e == 120 ? 2 : (e == 117 ? 4 : 8)
          hex = value.byteslice(i + 1, len).to_s
          out << utf8_char(hex.to_i(16))
          i += len
        else
          out << value.byteslice(i, 1).to_s
        end
      else
        out << value.byteslice(i, 1).to_s
      end
      i += 1
    end
    nil
  end

  # '...' where '' stands for '; nil when no quote closes it
  def self.parse_single_quoted(value)
    out = ""
    i = 1
    n = value.bytesize
    while i < n
      c = value.getbyte(i)
      if c == 39
        return out unless value.getbyte(i + 1) == 39
        out << "'"
        i += 2
        next
      end
      out << value.byteslice(i, 1).to_s
      i += 1
    end
    nil
  end

  # [a, b] or {k: v}, items split at commas outside quotes and brackets;
  # nil when nothing closes it
  def self.parse_flow(value, close, anchors)
    items = [] #: Array[String]
    depth = 0
    quote = 0
    start = 1
    i = 1
    n = value.bytesize
    while i < n
      c = value.getbyte(i)
      if quote != 0
        if c == 92 && quote == 34
          i += 1
        elsif c == quote
          quote = 0
        end
      elsif c == 34 || c == 39
        quote = c
      elsif c == 91 || c == 123
        depth += 1
      elsif (c == 93 || c == 125) && 0 < depth
        depth -= 1
      elsif c == close && depth == 0
        items << value.byteslice(start, i - start).to_s
        break
      elsif c == 44 && depth == 0 # ,
        items << value.byteslice(start, i - start).to_s
        start = i + 1
      end
      i += 1
    end
    return nil if n <= i
    # text after the closing bracket: a plain value, not a flow collection
    rest = value.byteslice(i + 1, n - i - 1).to_s
    return nil unless strip_comment(rest).strip.empty?
    if close == 93
      list = [] #: Array[untyped]
      j = 0
      while j < items.size
        item = items[j].strip
        # [] and the empty item after a trailing comma are no item
        list << parse_value(item, anchors) unless item.empty? && j == items.size - 1
        j += 1
      end
      list
    else
      hash = {} #: Hash[untyped, untyped]
      j = 0
      while j < items.size
        item = items[j].strip
        unless item.empty?
          k, v = split_key_value(item)
          assign(hash, k, v.nil? || v.empty? ? nil : parse_value(v, anchors))
        end
        j += 1
      end
      hash
    end
  end

  # the UTF-8 bytes of codepoint cp
  def self.utf8_char(cp)
    if 0x10ffff < cp || (0xd800 <= cp && cp <= 0xdfff)
      raise ArgumentError, "found invalid Unicode character escape code"
    end
    if cp < 0x80
      cp.chr
    elsif cp < 0x800
      (0xc0 | (cp >> 6)).chr + (0x80 | (cp & 0x3f)).chr
    elsif cp < 0x10000
      (0xe0 | (cp >> 12)).chr + (0x80 | ((cp >> 6) & 0x3f)).chr + (0x80 | (cp & 0x3f)).chr
    else
      (0xf0 | (cp >> 18)).chr + (0x80 | ((cp >> 12) & 0x3f)).chr +
        (0x80 | ((cp >> 6) & 0x3f)).chr + (0x80 | (cp & 0x3f)).chr
    end
  end

  def self.parse_list_item(string)
    parts = string.split(' ', 2)
    [nil, parts[1]&.strip]
  end

  def self.is_integer?(string)
    string = string.strip
    return false if string.empty?
    if string.getbyte(0) == 45 # '-'.ord
      string = string.byteslice(1, string.length - 1)
    end
    s = string.to_s
    s_len = s.bytesize
    ci = 0
    while ci < s_len
      char = s.getbyte(ci) or raise
      return false unless 48 <= char && char <= 57 # '0'.ord..'9'.ord
      ci += 1
    end
    true
  end

  def self.is_float?(string)
    string = string.strip
    return false if string.empty?
    parts = string.split('.')
    return false if parts.length != 2
    is_integer?(parts[0]) && is_integer?(parts[1])
  end

  def self.is_boolean?(string)
    string.downcase == 'true' || string.downcase == 'false'
  end

  def self.is_null?(string)
    string.downcase == 'null'
  end
end
