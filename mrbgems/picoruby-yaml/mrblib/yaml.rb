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

  def self.dump(ruby_object)
    serialize(ruby_object)
  end

#  private

  def self.parse(yaml_string)
    lines = yaml_string.split("\n")
    root = {}
    stack = [root]
    indent_stack = [-1]
    prev_indent = -1

    lines.each do |line|
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
          new_array = []
          if 1 < stack.size && stack[-2].is_a?(Hash)
            last_key = stack[-2].keys.last
            stack[-2][last_key] = new_array
          end
          stack[-1] = new_array
          current_object = new_array
        end
        if value.nil? || value.empty?
          new_item = {}
          current_object << new_item
          stack.push(new_item)
          indent_stack.push(indent)
        else
          current_object << parse_value(value)
        end
      else
        if value.nil? || value.empty?
          new_item = {}
          current_object[key] = new_item
          stack.push(new_item)
          indent_stack.push(indent)
        else
          current_object[key] = parse_value(value)
        end
      end

      prev_indent = indent
    end

    root
  end

  def self.parse_value(value)
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
      object.each do |key, value|
        yaml << "#{' ' * indent}#{key}:"
        if value.is_a?(Hash) || value.is_a?(Array)
          yaml << "\n" + serialize(value, indent + 2)
        else
          yaml << " #{serialize(value)}\n"
        end
      end
      yaml
    when Array
      yaml = ""
      # @type var object: Array
      object.each do |item|
        # @type var item: (Hash | Array)
        yaml << "#{' ' * indent}- "
        if item.is_a?(Hash) || item.is_a?(Array)
          yaml << "\n" + serialize(item, indent + 2)
        else
          yaml << "#{serialize(item)}\n"
        end
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
    [parts[0].strip, parts[1]&.strip]
  end

  def self.parse_list_item(string)
    parts = string.split(' ', 2)
    [nil, parts[1]&.strip]
  end

  def self.is_integer?(string)
    string = string.strip
    return false if string.empty?
    if string[0] == '-'
      string = string[1, string.length - 1]
    end
    string.to_s.each_char do |char|
      return false unless '0' <= char && char <= '9'
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
