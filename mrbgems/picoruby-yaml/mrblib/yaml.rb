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
          stack[-1] = new_array
          current_object = new_array
        end
        if value.nil? || value.empty?
          new_item = {} #: Hash[untyped, untyped]
          current_object << new_item
          stack.push(new_item)
          indent_stack.push(indent)
        elsif !quoted?(value) && (value.include?(': ') || value.end_with?(':'))
          # `- key: value` opens a Hash item. Its other keys follow at
          # the column of this key, so the item stays on the stack at the
          # dash's indent and a key of a nested value at its own column.
          item_key, item_value = split_key_value(value)
          new_item = {} #: Hash[untyped, untyped]
          current_object << new_item
          stack.push(new_item)
          indent_stack.push(indent)
          if item_value.nil? || item_value.empty?
            nested = {} #: Hash[untyped, untyped]
            new_item[item_key] = nested
            stack.push(nested)
            indent_stack.push(indent + line_strip.length - value.length)
          else
            new_item[item_key] = parse_value(item_value)
          end
        else
          current_object << parse_value(value)
        end
      else
        if value.nil? || value.empty?
          new_item = {} #: Hash[untyped, untyped]
          current_object[key] = new_item
          stack.push(new_item)
          indent_stack.push(indent)
        else
          current_object[key] = parse_value(value)
        end
      end

      prev_indent = indent
    end

    # A document whose first line is a list item replaced the root Hash.
    stack[0]
  end

  # A scalar, a quoted string ("..." with escapes, '...' with ''), a flow
  # sequence or mapping, ~ (nil) or a :symbol, as Psych reads them; a
  # # comment after a plain value is not part of it.
  def self.parse_value(value)
    value = value.strip
    first = value.getbyte(0)
    if first == 34 # "
      r = parse_double_quoted(value)
      return r if r
    elsif first == 39 # '
      r = parse_single_quoted(value)
      return r if r
    elsif first == 91 # [
      r = parse_flow(value, 93)
      return r if r
    elsif first == 123 # {
      r = parse_flow(value, 125)
      return r if r
    end
    value = strip_comment(value)
    return nil if value == "~"
    if first == 58 && 1 < value.bytesize && !value.include?(" ") # :symbol
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
    else
      object.to_s
    end
  end

  def self.count_leading_spaces(string)
    string.length - string.lstrip.length
  end

  def self.split_key_value(string)
    parts = string.split(':', 2)
    key = parts[0].strip
    key = parse_value(key).to_s if quoted?(key)
    [key, parts[1]&.strip]
  end

  def self.quoted?(string)
    first = string.getbyte(0)
    first == 34 || first == 39
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
  def self.parse_flow(value, close)
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
    if close == 93
      list = [] #: Array[untyped]
      j = 0
      while j < items.size
        item = items[j].strip
        list << parse_value(item) unless item.empty? && items.size == 1
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
          hash[k] = v.nil? || v.empty? ? nil : parse_value(v)
        end
        j += 1
      end
      hash
    end
  end

  # the UTF-8 bytes of codepoint cp
  def self.utf8_char(cp)
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
