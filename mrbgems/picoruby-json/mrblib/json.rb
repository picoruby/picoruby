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

  module Common
    def expect(char)
      if @json[@index] != char
        raise JSON::JSONError.new("Expected '#{char}' at index #{@index}, but got '#{@json[@index]}'")
      end
      @index += 1
    end

    def skip_whitespace
      while @index < @json.length && [' ', "\t", "\n", "\r"].include?(@json[@index])
        @index += 1
      end
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
      sequence.each_char do |char|
        expect(char)
      end
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
      keys.each do |key|
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
        @json = @json[@start_index, @index - @start_index]
        @json.strip!
        reset
        # p @json
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
      expect('"')
      string_start = @index
      while char = @json[@index]
        if char == '\\'
          @index += 2
        elsif char == '"'
          if need_return
            str = @json[string_start, @index - string_start]
          end
          @index += 1
          return str
        else
          @index += 1
        end
      end
      raise JSON::DiggerError.new("Unterminated string")
    end

    def dig_number
      while char = @json[@index]
        if char == '-' || char == '.' || char == 'e' || char == 'E' || ('0' <= char && char <= '9')
          @index += 1
        else
          break
        end
      end
    end

    def dig_object(key)
      push_stack(:object)
      skip_whitespace
      expect('{')
      while char = @json[@index]
        if char == '}'
          @index += 1
          if @stack[-1] == :object
            pop_stack
            break
          end
        end
        found_key = dig_string(true)
        if key && found_key == key
          skip_whitespace
          expect(':')
          @start_index = @index
          dig_value
          return
        else
          skip_whitespace
          expect(':')
          dig_value
          skip_whitespace
          if @json[@index] == '}'# && @stack[-1] == :object
            @index += 1
            pop_stack
            break
          end
          expect(',')
          skip_whitespace
        end
      end
      if key
        raise JSON::DiggerError.new("Key not found: #{key}")
      end
    end

    def dig_value
      skip_whitespace
      case @json[@index]
      when '{'
        dig_object(nil)
      when '['
        dig_array(nil)
      when '"'
        dig_string(false)
      when 't'
        skip_whitespace
        parse_true
      when 'f'
        skip_whitespace
        parse_false
      when 'n'
        skip_whitespace
        parse_null
      when '-', '0'
        dig_number
      else
        if @json[@index].to_i != 0 # from '1' to '9'
          dig_number
        else
          @index += 1
        end
      end
    end

    def dig_array(array_pos)
      push_stack(:array)
      skip_whitespace
      expect('[')
      skip_whitespace
      current_array_pos = 0
      @start_index = @index if array_pos
      while char = @json[@index]
        case char
        when ']'
          @index += 1
          break
        when ','
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
        else
          dig_value
        end
      end
      if array_pos && current_array_pos < array_pos
        JSON::DiggerError.new("Array index out of range")
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
      result += obj.map do |key, value|
        "#{generate_string(key)}:#{generate(value)}"
      end.join(',')
      result += '}'
    end

    def generate_array(obj)
      result = '['
      result += obj.map do |value|
        generate(value)
      end.join(',')
      result += ']'
    end

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
          result += char if char
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
      case @json[@index]
      when '{'
        parse_object
      when '['
        parse_array
      when '"'
        parse_string
      when '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
        parse_number
      when 't'
        parse_true
      when 'f'
        parse_false
      when 'n'
        parse_null
      else
        raise JSON::ParserError.new("Unexpected character at index #{@index}")
      end
    end

    # private

    def parse_object
      result = {}
      @index += 1  # Skip '{'
      skip_whitespace

      unless @json[@index] == '}'
        loop do
          key = parse_string
          skip_whitespace
          expect(':')
          skip_whitespace
          value = parse
          result[key] = value
          skip_whitespace
          break if @json[@index] == '}'
          expect(',')
          skip_whitespace
        end
      end

      @index += 1  # Skip '}'
      result
    end

    def parse_array
      result = []
      @index += 1  # Skip '['
      skip_whitespace

      unless @json[@index] == ']'
        loop do
          value = parse
          result << value
          skip_whitespace
          break if @json[@index] == ']'
          expect(',')
          skip_whitespace
        end
      end

      @index += 1  # Skip ']'
      result
    end

    def parse_string
      @index += 1  # Skip opening quote
      result = ''
      while @json[@index] != '"'
        if @json[@index] == '\\'
          if snip = @json[@index, 2]
            result += snip
            @index += snip.length
          end
        else
          result += @json[@index].to_s
          @index += 1
        end
        if @index >= @json.length
          raise JSON::ParserError.new("Unterminated string")
        end
      end
      @index += 1  # Skip closing quote
      replace_escape_sequence(result)
    end

    def replace_escape_sequence(str)
      result = ''
      i = 0
      str_len = str.length
      while i < str_len
        char = str[i]
        if char == '\\'
          i += 1
          case str[i]
          when '"'
            result += '"'
          when '\\'
            result += '\\'
          when '/'
            result += '/'
          when 'b'
            result += "\b"
          when 'f'
            result += "\f"
          when 'n'
            result += "\n"
          when 'r'
            result += "\r"
          when 't'
            result += "\t"
          when 'u'
            # TODO: Implement Unicode escape sequence
            # result += [str[i + 2, 4].to_i(16)].pack('U')
          when nil
            raise JSON::ParserError.new("Unterminated escape sequence")
          else
            raise JSON::ParserError.new("Unknown escape sequence: \\#{str[i]}")
          end
        elsif char.nil?
          raise JSON::ParserError.new("Unterminated string")
        else
          result += char
        end
        i += 1
      end
      result
    end

    def parse_number
      start = @index
      is_float = false
      is_negative = @json[@index] == '-'
      @index += 1 if is_negative

      # Integer part
      while @index < @json.length && is_digit?(@json[@index])
        @index += 1
      end

      # Fractional part
      if @index < @json.length && @json[@index] == '.'
        is_float = true
        @index += 1
        while @index < @json.length && is_digit?(@json[@index])
          @index += 1
        end
      end

      # Exponent part
      if @index < @json.length && (@json[@index] == 'e' || @json[@index] == 'E')
        is_float = true
        @index += 1
        @index += 1 if @index < @json.length && (@json[@index] == '+' || @json[@index] == '-')
        while @index < @json.length && is_digit?(@json[@index])
          @index += 1
        end
      end

      if is_float
        parse_float(start, @index)
      else
        parse_integer(start, @index)
      end
    end

    def parse_integer(start, end_index)
      result = 0
      is_negative = @json[start] == '-'
      start += 1 if is_negative

      (start...end_index).each do |i|
        # @type var ord: Integer
        ord = @json[i]&.ord
        result = result * 10 + (ord - '0'.ord)
      end

      is_negative ? -result : result
    end

    def parse_float(start, end_index)
      result = 0.0
      decimal_divider = 1.0
      exponent = 0
      is_negative = @json[start] == '-'
      exponent_negative = false
      parsing_exponent = false
      start += 1 if is_negative

      (start...end_index).each do |i|
        case @json[i]
        when '0'..'9'
          # @type var ord: Integer
          ord = @json[i]&.ord
          if parsing_exponent
            exponent = exponent * 10 + (ord - '0'.ord)
          elsif decimal_divider == 1.0
            result = result * 10 + (ord - '0'.ord)
          else
            decimal_divider *= 10
            result += (ord - '0'.ord) / decimal_divider
          end
        when '.'
          # Do nothing, just move to the next character
        when 'e', 'E'
          parsing_exponent = true
        when '-'
          exponent_negative = true if parsing_exponent
        when '+'
          # Do nothing for positive exponent
        end
      end

      result *= (10.0 ** (exponent_negative ? -exponent : exponent))
      is_negative ? -result : result
    end

    def is_digit?(char)
      return false unless char
      '0' <= char && char <= '9'
    end

  end
end

