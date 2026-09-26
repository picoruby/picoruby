#
# JSON library for PicoRuby
# This is a simple JSON parser and generator for PicoRuby.
# It is designed to be small and simple, not to be fast or complete.
#
# Author: Hitoshi HASUMI
# License: MIT
#


module JSON

  class JSONError < StandardError; end
  class ParserError < JSONError; end
  class GeneratorError < JSONError; end
  class DiggerError < JSONError; end

  # Parser and Digger scan the source by byte offset with getbyte/byteslice.
  # With MRB_UTF8_STRING, String#[] by character index walks the string from
  # its head whenever it holds a multibyte character, which made a whole parse
  # quadratic in the input length.
  module Common
    def expect(byte)
      if @json.getbyte(@index) != byte
        raise JSON::JSONError.new("Expected '#{byte.chr}' at index #{@index}, but got '#{@json.byteslice(@index, 1)}'")
      end
      @index += 1
    end

    def skip_whitespace
      json = @json
      index = @index
      while true
        byte = json.getbyte(index)
        # ' ', "\t", "\n", "\r"
        break unless byte == 32 || byte == 9 || byte == 10 || byte == 13
        index += 1
      end
      @index = index
    end

    def parse_true
      expect_sequence('true')
      true
    end

    def parse_false
      expect_sequence('false')
      false
    end

    def parse_null
      expect_sequence('null')
      nil
    end

    def expect_sequence(sequence)
      len = sequence.bytesize
      if @json.byteslice(@index, len) != sequence
        raise JSON::JSONError.new("Expected '#{sequence}' at index #{@index}")
      end
      @index += len
    end
  end

  def self.parse(json)
    JSON::Parser.new(json).parse
  end

  def self.generate(obj)
    JSON::Generator.new(obj).generate
  end

  # Digger class is to dig into the JSON object especially dedicated
  # to the small memory environment.
  # It does not parse the whole JSON object but it scans the JSON string
  # and extract a part of the JSON object.
  #
  # Usage:
  #   json = <<~JSON
  #     [{"id":"02e548eb-900d-4de4-93d8-xxxxxxxxxxxx","device":{"name":"MyName","id":"9b098976-98fb-439f-ac09-xxxxxxxxxxxx","created_at":"2024-09-19T01:08:25Z","updated_at":"2024-09-19T08:53:00Z","mac_address":"f0:08:d1:ea:da:00","bt_mac_address":"f0:08:d1:ea:da:02","serial_number":"4W121010002448","firmware_version":"Remo-E-lite/1.10.0","temperature_offset":0,"humidity_offset":0},"model":{"id":"7f3de26b-0afa-44fe-8680-xxxxxxxxxxxx","manufacturer":"","name":"Smart Meter","image":"ico_smartmeter"},"type":"EL_SMART_METER","nickname":"NyNickname","image":"ico_smartmeter","settings":null,"aircon":null,"signals":[],"smart_meter":{"echonetlite_properties":[{"name":"coefficient","epc":211,"val":"1","updated_at":"2024-09-20T01:44:15Z"},{"name":"cumulative_electric_energy_effective_digits","epc":215,"val":"6","updated_at":"2024-09-20T01:44:15Z"},{"name":"normal_direction_cumulative_electric_energy","epc":224,"val":"80481","updated_at":"2024-09-20T01:44:15Z"},{"name":"cumulative_electric_energy_unit","epc":225,"val":"1","updated_at":"2024-09-20T01:44:15Z"},{"name":"reverse_direction_cumulative_electric_energy","epc":227,"val":"9","updated_at":"2024-09-20T01:44:15Z"},{"name":"measured_instantaneous","epc":231,"val":"599","updated_at":"2024-09-20T01:46:14Z"}]}}]
  #   JSON
  #
  #   json = '[{"device":{"name":"Remo"}}, {"device":{"name":null}}]'
  #   JSON::Digger.new(json).dig(0, 'device', 'name')
  #   => Digger object
  #   JSON::Digger.new(json).dig(0, 'device', 'name').parse
  #   => "Remo"
  #   JSON::Digger.new(json).dig(0, 'device')
  #   => Digger object
  #   JSON::Digger.new(json).dig(0, 'device').parse
  #   => {"name"=>"Remo"}
  #   JSON::Digger.new(json).dig(1, 'device', 'name').parse
  #   => nil
  #   JSON::Digger.new(json).dig(0, 'device').dig('name').parse
  #   => "Remo"
  #   JSON::Digger.new(json).dig(3, 'device', 'name')
  #   => JSON::DiggerError: Array index out of range
  #   JSON::Digger.new(json).dig(1, '___device', 'name')
  #   => JSON::DiggerError: Key not found: ___device
  #
  class Digger
    include JSON::Common

    def initialize(json)
      @json = json
      reset
    end

    # attr_reader :json

    def dig(*keys)
      ki = 0
      while ki < keys.size
        key = keys[ki]
        case key
        when Integer
          if key < 0
            raise ArgumentError.new("Negative index is not supported")
          end
          # @type var key: Integer
          dig_array(key)
        when String
          # @type var key: String
          dig_object(key)
        else
          raise ArgumentError.new("Unsupported type: #{key.class}")
        end
        @json = @json.byteslice(@start_index, @index - @start_index) || ""
        @json.strip!
        reset
        # p @json
        ki += 1
      end
      return self
    end

    def parse
      JSON.parse(@json)
    end

    # private

    def reset
      @start_index = 0
      @index = 0
      @stack = []
    end

    def push_stack(type)
      @stack.push(type)
      #puts "push_stack: #{@stack}, index: #{@index}"
    end

    def pop_stack
      @stack.pop
      #puts "pop_stack: #{@stack}, index: #{@index}"
    end

    def dig_string(need_return)
      skip_whitespace
      expect(34) # '"'
      json = @json
      string_start = @index
      index = string_start
      while byte = json.getbyte(index)
        if byte == 92 # '\\'
          index += 2
        elsif byte == 34 # '"'
          @index = index + 1
          if need_return
            return json.byteslice(string_start, index - string_start)
          end
          return nil
        else
          index += 1
        end
      end
      @index = index
      raise JSON::DiggerError.new("Unterminated string")
    end

    def dig_number
      json = @json
      index = @index
      while byte = json.getbyte(index)
        # '-', '+', '.', 'e', 'E', '0'..'9'
        if byte == 45 || byte == 43 || byte == 46 || byte == 101 || byte == 69 || (48 <= byte && byte <= 57)
          index += 1
        else
          break
        end
      end
      @index = index
    end

    def dig_object(key)
      push_stack(:object)
      skip_whitespace
      expect(123) # '{'
      skip_whitespace
      while byte = @json.getbyte(@index)
        if byte == 125 # '}'
          @index += 1
          if @stack[-1] == :object
            pop_stack
            break
          end
        end
        found_key = dig_string(true)
        if key && found_key == key
          skip_whitespace
          expect(58) # ':'
          @start_index = @index
          dig_value
          return
        else
          skip_whitespace
          expect(58) # ':'
          dig_value
          skip_whitespace
          if @json.getbyte(@index) == 125 # '}' # && @stack[-1] == :object
            @index += 1
            pop_stack
            break
          end
          expect(44) # ','
          skip_whitespace
        end
      end
      if key
        raise JSON::DiggerError.new("Key not found: #{key}")
      end
    end

    def dig_value
      skip_whitespace
      byte = @json.getbyte(@index)
      case byte
      when 123 # '{'
        dig_object(nil)
      when 91 # '['
        dig_array(nil)
      when 34 # '"'
        dig_string(false)
      when 116 # 't'
        parse_true
      when 102 # 'f'
        parse_false
      when 110 # 'n'
        parse_null
      when 45, 48 # '-', '0'
        dig_number
      else
        if byte && 49 <= byte && byte <= 57 # from '1' to '9'
          dig_number
        else
          @index += 1
        end
      end
    end

    def dig_array(array_pos)
      push_stack(:array)
      skip_whitespace
      expect(91) # '['
      skip_whitespace
      current_array_pos = 0
      found = false
      @start_index = @index if array_pos
      while byte = @json.getbyte(@index)
        case byte
        when 93 # ']'
          @index += 1
          pop_stack
          break
        when 44 # ','
          @index += 1
          if @stack[-1] == :array
            if array_pos
              if current_array_pos == array_pos
                pop_stack
                break
              end
              @start_index = @index
              current_array_pos += 1
            end
          end
        when 32, 9, 10, 13 # ' ', "\t", "\n", "\r"
          # dig_value would take a ',' after whitespace for a value
          @index += 1
        else
          found = true if current_array_pos == array_pos
          dig_value
        end
      end
      if array_pos && !found
        raise JSON::DiggerError.new("Array index out of range")
      end
    end
  end

  class Generator
    include JSON::Common

    def initialize(obj)
      @obj = obj
    end

    def generate(obj = @obj)
      case obj
      when Hash
        generate_object(obj)
      when Array
        generate_array(obj)
      when String, Symbol
        generate_string(obj)
      when Integer, Float
        generate_number(obj)
      when TrueClass
        "true"
      when FalseClass
        "false"
      when NilClass
        'null'
      else
        generate_string(obj.to_s)
      end
    end

    # private

    def generate_object(obj)
      result = '{'
      keys = obj.keys
      i = 0
      while i < keys.size
        result += ',' if 0 < i
        key = keys[i]
        result += "#{generate_string(key)}:#{generate(obj[key])}"
        i += 1
      end
      result += '}'
    end

    def generate_array(obj)
      result = '['
      i = 0
      while i < obj.size
        result += ',' if 0 < i
        result += generate(obj[i])
        i += 1
      end
      result += ']'
    end

    HEX_CHARS = "0123456789abcdef"

    def generate_string(obj)
      # Manually escape special characters since PicoRuby does not support gsub nor Regexp
      str = obj.to_s
      result = '"'
      i = 0
      while i < str.length
        char = str[i]
        case char
        when '"'
          result += '\\"'
        when '\\'
          result += '\\\\'
        when "\b"
          result += '\\b'
        when "\f"
          result += '\\f'
        when "\n"
          result += '\\n'
        when "\r"
          result += '\\r'
        when "\t"
          result += '\\t'
        else
          if char
            code = char.ord
            if code < 0x20
              # Escape control characters as \u00XX
              result += '\\u00'
              result += (HEX_CHARS[code >> 4] || '0')
              result += (HEX_CHARS[code & 0x0f] || '0')
            else
              result += char
            end
          end
        end
        i += 1
      end
      result += '"'
      result
    end

    def generate_number(obj)
      obj.to_s
    end
  end

  class Parser
    include JSON::Common

    def initialize(json)
      @json = json
      @index = 0
    end

    def parse
      skip_whitespace
      byte = @json.getbyte(@index)
      case byte
      when 123 # '{'
        parse_object
      when 91 # '['
        parse_array
      when 34 # '"'
        parse_string
      when 45, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 # '-', '0'..'9'
        parse_number
      when 116 # 't'
        parse_true
      when 102 # 'f'
        parse_false
      when 110 # 'n'
        parse_null
      else
        raise JSON::ParserError.new("Unexpected character at index #{@index}")
      end
    end

    # private

    def parse_object
      result = {} #: Hash[String, untyped]
      @index += 1  # Skip '{'
      skip_whitespace

      unless @json.getbyte(@index) == 125 # '}'
        while true
          if @json.getbyte(@index) != 34 # '"'
            raise JSON::ParserError.new("Expected string key at index #{@index}")
          end
          key = parse_string
          skip_whitespace
          expect(58) # ':'
          skip_whitespace
          value = parse
          result[key] = value
          skip_whitespace
          break if @json.getbyte(@index) == 125 # '}'
          expect(44) # ','
          skip_whitespace
        end
      end

      @index += 1  # Skip '}'
      result
    end

    def parse_array
      result = [] #: Array[untyped]
      @index += 1  # Skip '['
      skip_whitespace

      unless @json.getbyte(@index) == 93 # ']'
        while true
          value = parse
          result << value
          skip_whitespace
          break if @json.getbyte(@index) == 93 # ']'
          expect(44) # ','
          skip_whitespace
        end
      end

      @index += 1  # Skip ']'
      result
    end

    # Copies each run of plain bytes with one byteslice, and decodes an escape
    # sequence where it is met.
    def parse_string
      json = @json
      index = @index + 1  # Skip opening quote
      run_start = index
      result = ''
      while true
        byte = json.getbyte(index)
        if byte == 34 # '"'
          break
        elsif byte == 92 # '\\'
          result << json.byteslice(run_start, index - run_start).to_s if run_start < index
          index = parse_escape(result, index + 1)
          run_start = index
        elsif byte.nil?
          raise JSON::ParserError.new("Unterminated string")
        else
          index += 1
        end
      end
      if run_start < index
        run = json.byteslice(run_start, index - run_start).to_s
        # A string with no escape needs no copy of its own
        if result.empty?
          result = run
        else
          result << run
        end
      end
      @index = index + 1  # Skip closing quote
      result
    end

    # Appends the character an escape sequence stands for to result, and
    # returns the index just past the sequence. index points past the '\\'.
    def parse_escape(result, index)
      case @json.getbyte(index)
      when 34 # '"'
        result << '"'
      when 92 # '\\'
        result << '\\'
      when 47 # '/'
        result << '/'
      when 98 # 'b'
        result << "\b"
      when 102 # 'f'
        result << "\f"
      when 110 # 'n'
        result << "\n"
      when 114 # 'r'
        result << "\r"
      when 116 # 't'
        result << "\t"
      when 117 # 'u'
        code = parse_hex4(index + 1)
        index += 4
        # A high surrogate followed by \uDC00-\uDFFF spells one character
        if 0xD800 <= code && code <= 0xDBFF &&
            @json.getbyte(index + 1) == 92 && @json.getbyte(index + 2) == 117
          low = parse_hex4(index + 3)
          if 0xDC00 <= low && low <= 0xDFFF
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00)
            index += 6
          end
        end
        result << utf8_encode(code)
      when nil
        raise JSON::ParserError.new("Unterminated escape sequence")
      else
        raise JSON::ParserError.new("Unknown escape sequence: \\#{@json.byteslice(index, 1)}")
      end
      index + 1
    end

    def parse_hex4(index)
      json = @json
      code = 0
      i = 0
      while i < 4
        byte = json.getbyte(index + i)
        if byte.nil?
          raise JSON::ParserError.new("Incomplete unicode escape sequence")
        elsif 48 <= byte && byte <= 57 # '0'..'9'
          code = (code << 4) + byte - 48
        elsif 97 <= byte && byte <= 102 # 'a'..'f'
          code = (code << 4) + byte - 87
        elsif 65 <= byte && byte <= 70 # 'A'..'F'
          code = (code << 4) + byte - 55
        else
          raise JSON::ParserError.new("Invalid hex in unicode escape: #{json.byteslice(index + i, 1)}")
        end
        i += 1
      end
      code
    end

    def utf8_encode(code)
      if code < 0x80
        [code].pack('C*')
      elsif code < 0x800
        [0xC0 | (code >> 6), 0x80 | (code & 0x3F)].pack('C*')
      elsif code < 0x10000
        [0xE0 | (code >> 12), 0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F)].pack('C*')
      else
        [0xF0 | (code >> 18), 0x80 | ((code >> 12) & 0x3F),
         0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F)].pack('C*')
      end
    end

    def parse_number
      json = @json
      start = @index
      index = start
      is_float = false
      index += 1 if json.getbyte(index) == 45 # '-'

      # Integer part
      index = skip_digits(index)

      # Fractional part
      if json.getbyte(index) == 46 # '.'
        is_float = true
        index = skip_digits(index + 1)
      end

      # Exponent part
      byte = json.getbyte(index)
      if byte == 101 || byte == 69 # 'e', 'E'
        is_float = true
        index += 1
        byte = json.getbyte(index)
        index += 1 if byte == 43 || byte == 45 # '+', '-'
        index = skip_digits(index)
      end

      @index = index
      if is_float
        parse_float(start, index)
      else
        parse_integer(start, index)
      end
    end

    def skip_digits(index)
      json = @json
      while true
        byte = json.getbyte(index)
        break unless byte && 48 <= byte && byte <= 57 # '0'..'9'
        index += 1
      end
      index
    end

    # start/end_index are byte offsets
    def parse_integer(start, end_index)
      json = @json
      result = 0
      is_negative = json.getbyte(start) == 45 # '-'
      start += 1 if is_negative

      i = start
      while i < end_index
        result = result * 10 + (json.getbyte(i).to_i - 48) # '0'.ord is 48
        i += 1
      end

      is_negative ? -result : result
    end

    # start/end_index are byte offsets
    def parse_float(start, end_index)
      json = @json
      result = 0.0
      decimal_divider = 1.0
      exponent = 0
      is_negative = json.getbyte(start) == 45 # '-'
      exponent_negative = false
      parsing_exponent = false
      start += 1 if is_negative

      i = start
      while i < end_index
        byte = json.getbyte(i).to_i
        if 48 <= byte && byte <= 57 # '0'..'9'
          if parsing_exponent
            exponent = exponent * 10 + (byte - 48)
          elsif decimal_divider == 1.0
            result = result * 10 + (byte - 48)
          else
            result += (byte - 48) / decimal_divider
            decimal_divider *= 10
          end
        elsif byte == 46 # '.'
          decimal_divider = 10.0
        elsif byte == 101 || byte == 69 # 'e', 'E'
          parsing_exponent = true
        elsif byte == 45 # '-'
          exponent_negative = true if parsing_exponent
        end
        i += 1
      end

      result *= (10.0 ** (exponent_negative ? -exponent : exponent))
      is_negative ? -result : result
    end

  end
end
