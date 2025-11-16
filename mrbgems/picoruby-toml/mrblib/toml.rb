#
# TOML library for PicoRuby
# This is a simple TOML parser and generator for PicoRuby.
# It is designed to be small and simple, not to be fast or complete.
#
# Author: Hitoshi HASUMI
# License: MIT
#

module TOML
  class TOMLError < StandardError; end
  class ParseError < TOMLError; end
  class GeneratorError < TOMLError; end

  def self.parse(toml)
    Parser.new(toml).parse
  end

  def self.generate(obj)
    Generator.new(obj).generate
  end

  class Parser
    def initialize(toml)
      @toml = toml
      @lines = toml.split("\n")
      @line_num = 0
      @result = {}
      @current_table = @result
      @table_path = []
    end

    def parse
      @lines.each do |line|
        @line_num += 1
        parse_line(line)
      end
      @result
    end

    private

    def parse_line(line)
      # Remove inline comments (but not in strings)
      line = strip_comment(line)

      # Skip empty lines
      line_stripped = line.strip
      return if line_stripped.empty?

      # Check line type
      if line_stripped.start_with?("[[")
        # Array of tables
        parse_array_table(line_stripped)
      elsif line_stripped.start_with?("[")
        # Table header
        parse_table_header(line_stripped)
      else
        # Key-value pair
        parse_key_value(line_stripped)
      end
    end

    def strip_comment(line)
      result = ""
      in_string = false
      escape_next = false
      i = 0

      while i < line.length
        char = line[i]

        if escape_next
          result += char
          escape_next = false
        elsif char == '\\'
          result += char
          escape_next = true
        elsif char == '"'
          result += char
          in_string = !in_string
        elsif char == '#' && !in_string
          break
        else
          result += char
        end

        i += 1
      end

      result
    end

    def parse_table_header(line)
      # Extract table name between [ and ]
      if line[-1] != ']'
        raise ParseError.new("Invalid table header at line #{@line_num}")
      end

      table_name = line[1..-2].strip
      @table_path = table_name.split('.')

      # Navigate to or create the table
      @current_table = @result
      @table_path.each do |key|
        @current_table[key] ||= {}
        @current_table = @current_table[key]
      end
    end

    def parse_array_table(line)
      # Extract table name between [[ and ]]
      if !line.end_with?("]]")
        raise ParseError.new("Invalid array table header at line #{@line_num}")
      end

      table_name = line[2..-3].strip
      path = table_name.split('.')

      # Navigate to parent and create array element
      current = @result
      path[0..-2].each do |key|
        current[key] ||= {}
        current = current[key]
      end

      last_key = path[-1]
      current[last_key] ||= []

      new_table = {}
      current[last_key] << new_table
      @current_table = new_table
      @table_path = path
    end

    def parse_key_value(line)
      # Find the = sign
      eq_pos = find_equals_sign(line)

      if eq_pos.nil?
        raise ParseError.new("Invalid key-value pair at line #{@line_num}")
      end

      key = line[0...eq_pos].strip
      value_str = line[eq_pos + 1..-1].strip

      # Parse the key (may be quoted)
      key = parse_key(key)

      # Parse the value
      value = parse_value(value_str)

      @current_table[key] = value
    end

    def find_equals_sign(line)
      in_string = false
      escape_next = false

      line.length.times do |i|
        char = line[i]

        if escape_next
          escape_next = false
        elsif char == '\\'
          escape_next = true
        elsif char == '"'
          in_string = !in_string
        elsif char == '=' && !in_string
          return i
        end
      end

      nil
    end

    def parse_key(key)
      # Remove quotes if quoted
      if key.start_with?('"') && key.end_with?('"')
        return parse_basic_string(key)
      end
      key
    end

    def parse_value(value_str)
      # Determine value type
      first_char = value_str[0]

      case first_char
      when '"'
        # String
        if value_str.start_with?('"""')
          parse_multiline_string(value_str)
        else
          parse_basic_string(value_str)
        end
      when "'"
        # Literal string
        if value_str.start_with?("'''")
          parse_multiline_literal_string(value_str)
        else
          parse_literal_string(value_str)
        end
      when '['
        # Array
        parse_array(value_str)
      when 't', 'f'
        # Boolean
        parse_boolean(value_str)
      when '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
        # Number or datetime
        parse_number_or_datetime(value_str)
      else
        raise ParseError.new("Unknown value type at line #{@line_num}: #{value_str}")
      end
    end

    def parse_basic_string(str)
      # Remove quotes
      if !str.start_with?('"') || !str.end_with?('"')
        raise ParseError.new("Invalid string at line #{@line_num}")
      end

      content = str[1..-2]
      unescape_string(content)
    end

    def parse_literal_string(str)
      # Literal strings don't process escape sequences
      if !str.start_with?("'") || !str.end_with?("'")
        raise ParseError.new("Invalid literal string at line #{@line_num}")
      end

      str[1..-2]
    end

    def parse_multiline_string(str)
      if !str.start_with?('"""') || !str.end_with?('"""')
        raise ParseError.new("Invalid multiline string at line #{@line_num}")
      end

      content = str[3..-4]
      unescape_string(content)
    end

    def parse_multiline_literal_string(str)
      if !str.start_with?("'''") || !str.end_with?("'''")
        raise ParseError.new("Invalid multiline literal string at line #{@line_num}")
      end

      str[3..-4]
    end

    def unescape_string(str)
      result = ""
      i = 0

      while i < str.length
        if str[i] == '\\'
          i += 1
          case str[i]
          when 'b'
            result += "\b"
          when 't'
            result += "\t"
          when 'n'
            result += "\n"
          when 'f'
            result += "\f"
          when 'r'
            result += "\r"
          when '"'
            result += '"'
          when '\\'
            result += '\\'
          else
            result += str[i] if str[i]
          end
        else
          result += str[i]
        end
        i += 1
      end

      result
    end

    def parse_boolean(str)
      case str
      when "true"
        true
      when "false"
        false
      else
        raise ParseError.new("Invalid boolean at line #{@line_num}: #{str}")
      end
    end

    def parse_number_or_datetime(str)
      # Check for datetime (contains T or -)
      if str.include?('T') || (str.include?('-') && str.count('-') >= 2)
        # Simplified datetime parsing - just return as string for now
        return str
      end

      # Check for float
      if str.include?('.') || str.include?('e') || str.include?('E')
        parse_float(str)
      else
        parse_integer(str)
      end
    end

    def parse_integer(str)
      # Remove underscores
      str = str.tr('_', '')

      # Handle different bases
      if str.start_with?('0x')
        str[2..-1].to_i(16)
      elsif str.start_with?('0o')
        str[2..-1].to_i(8)
      elsif str.start_with?('0b')
        str[2..-1].to_i(2)
      else
        str.to_i
      end
    end

    def parse_float(str)
      # Remove underscores
      str = str.tr('_', '')
      str.to_f
    end

    def parse_array(str)
      # Simple array parser
      if !str.start_with?('[') || !str.end_with?(']')
        raise ParseError.new("Invalid array at line #{@line_num}")
      end

      content = str[1..-2].strip
      return [] if content.empty?

      # Split by commas (not in strings or nested arrays)
      elements = split_array_elements(content)

      elements.map { |elem| parse_value(elem.strip) }
    end

    def split_array_elements(str)
      elements = []
      current = ""
      depth = 0
      in_string = false
      escape_next = false

      str.length.times do |i|
        char = str[i]

        if escape_next
          current += char
          escape_next = false
        elsif char == '\\'
          current += char
          escape_next = true
        elsif char == '"' && !in_string
          current += char
          in_string = true
        elsif char == '"' && in_string
          current += char
          in_string = false
        elsif (char == '[' || char == '{') && !in_string
          current += char
          depth += 1
        elsif (char == ']' || char == '}') && !in_string
          current += char
          depth -= 1
        elsif char == ',' && depth == 0 && !in_string
          elements << current
          current = ""
        else
          current += char
        end
      end

      elements << current if current.length > 0
      elements
    end
  end

  class Generator
    def initialize(obj)
      @obj = obj
    end

    def generate(obj = @obj, prefix = [])
      result = ""

      # Separate simple key-values from tables
      simple_keys = []
      table_keys = []

      obj.each do |key, value|
        if value.is_a?(Hash)
          table_keys << key
        elsif value.is_a?(Array) && value.all? { |v| v.is_a?(Hash) }
          table_keys << key
        else
          simple_keys << key
        end
      end

      # Generate simple key-values
      simple_keys.each do |key|
        result += "#{format_key(key)} = #{format_value(obj[key])}\n"
      end

      # Generate tables
      table_keys.each do |key|
        value = obj[key]
        new_prefix = prefix + [key]

        if value.is_a?(Array) && value.all? { |v| v.is_a?(Hash) }
          # Array of tables
          value.each do |table|
            result += "\n" unless result.empty?
            result += "[[#{new_prefix.join('.')}]]\n"
            result += generate(table, new_prefix)
          end
        elsif value.is_a?(Hash)
          # Regular table
          result += "\n" unless result.empty?
          result += "[#{new_prefix.join('.')}]\n"
          result += generate(value, new_prefix)
        end
      end

      result
    end

    def format_key(key)
      # Quote key if it contains special characters
      if key.match?(/[^a-zA-Z0-9_-]/)
        "\"#{key}\""
      else
        key
      end
    end

    def format_value(value)
      case value
      when String
        format_string(value)
      when Integer, Float
        value.to_s
      when TrueClass
        "true"
      when FalseClass
        "false"
      when Array
        format_array(value)
      when Hash
        # Inline table (not implemented for simplicity)
        raise GeneratorError.new("Nested hashes should be separate tables")
      when NilClass
        raise GeneratorError.new("TOML does not support null values")
      else
        value.to_s
      end
    end

    def format_string(str)
      # Use basic string with escaping
      escaped = str.tr('"', '\"')
      escaped = escaped.tr('\\', '\\\\')
      escaped = escaped.tr("\n", '\\n')
      escaped = escaped.tr("\t", '\\t')
      "\"#{escaped}\""
    end

    def format_array(arr)
      "[" + arr.map { |elem| format_value(elem) }.join(", ") + "]"
    end
  end
end
