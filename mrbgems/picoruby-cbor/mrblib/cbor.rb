#
# CBOR library for PicoRuby
# This is a pure Ruby implementation of CBOR (Concise Binary Object Representation).
# RFC 8949: https://tools.ietf.org/html/rfc8949
#
# Author: Hitoshi HASUMI
# License: MIT
#

module CBOR
  class CBORError < StandardError; end
  class EncodeError < CBORError; end
  class DecodeError < CBORError; end

  # Major types
  TYPE_UINT = 0
  TYPE_NEGINT = 1
  TYPE_BYTES = 2
  TYPE_TEXT = 3
  TYPE_ARRAY = 4
  TYPE_MAP = 5
  TYPE_TAG = 6
  TYPE_SIMPLE = 7

  # Simple values
  SIMPLE_FALSE = 20
  SIMPLE_TRUE = 21
  SIMPLE_NULL = 22
  SIMPLE_UNDEFINED = 23

  # Additional information values
  AI_DIRECT = 0..23
  AI_UINT8 = 24
  AI_UINT16 = 25
  AI_UINT32 = 26
  AI_UINT64 = 27
  AI_INDEFINITE = 31

  def self.encode(obj)
    Encoder.new.encode(obj)
  end

  def self.decode(data)
    Decoder.new(data).decode
  end

  # Tagged value wrapper
  class Tagged
    attr_accessor :tag, :value

    def initialize(tag, value)
      @tag = tag
      @value = value
    end

    def ==(other)
      other.is_a?(Tagged) && @tag == other.tag && @value == other.value
    end
  end

  class Encoder
    def initialize
      @output = ""
    end

    def encode(obj)
      @output = ""
      encode_value(obj)
      @output
    end

    private

    def encode_value(obj)
      case obj
      when Integer
        encode_integer(obj)
      when Float
        encode_float(obj)
      when String
        encode_string(obj)
      when Array
        encode_array(obj)
      when Hash
        encode_map(obj)
      when TrueClass
        encode_simple(SIMPLE_TRUE)
      when FalseClass
        encode_simple(SIMPLE_FALSE)
      when NilClass
        encode_simple(SIMPLE_NULL)
      when Tagged
        encode_tagged(obj)
      else
        raise EncodeError.new("Unsupported type: #{obj.class}")
      end
    end

    def encode_integer(value)
      if value >= 0
        # Unsigned integer (major type 0)
        encode_type_value(TYPE_UINT, value)
      else
        # Negative integer (major type 1)
        # CBOR represents -n as type 1 with value n-1
        encode_type_value(TYPE_NEGINT, -1 - value)
      end
    end

    def encode_type_value(major_type, value)
      if value < 24
        # Direct encoding
        @output += [(major_type << 5) | value].pack("C")
      elsif value < 256
        # 1-byte uint
        @output += [(major_type << 5) | AI_UINT8].pack("C")
        @output += [value].pack("C")
      elsif value < 65536
        # 2-byte uint
        @output += [(major_type << 5) | AI_UINT16].pack("C")
        @output += [value].pack("n")
      elsif value < 4294967296
        # 4-byte uint
        @output += [(major_type << 5) | AI_UINT32].pack("C")
        @output += [value].pack("N")
      else
        # 8-byte uint
        @output += [(major_type << 5) | AI_UINT64].pack("C")
        high = value >> 32
        low = value & 0xffffffff
        @output += [high, low].pack("NN")
      end
    end

    def encode_float(value)
      # Use 64-bit float (major type 7, additional info 27)
      @output += [(TYPE_SIMPLE << 5) | 27].pack("C")
      @output += [value].pack("G")
    end

    def encode_string(value)
      # Text string (major type 3)
      length = value.length
      encode_type_value(TYPE_TEXT, length)
      @output += value
    end

    def encode_array(array)
      # Array (major type 4)
      encode_type_value(TYPE_ARRAY, array.length)
      array.each do |elem|
        encode_value(elem)
      end
    end

    def encode_map(hash)
      # Map (major type 5)
      encode_type_value(TYPE_MAP, hash.length)
      hash.each do |key, value|
        encode_value(key)
        encode_value(value)
      end
    end

    def encode_simple(simple_value)
      # Simple value (major type 7)
      @output += [(TYPE_SIMPLE << 5) | simple_value].pack("C")
    end

    def encode_tagged(tagged)
      # Tag (major type 6)
      encode_type_value(TYPE_TAG, tagged.tag)
      encode_value(tagged.value)
    end
  end

  class Decoder
    def initialize(data)
      @data = data
      @offset = 0
    end

    def decode
      decode_value
    end

    private

    def decode_value
      raise DecodeError.new("Unexpected end of data") if @offset >= @data.length

      initial_byte = read_byte
      major_type = (initial_byte >> 5) & 0x07
      additional_info = initial_byte & 0x1f

      case major_type
      when TYPE_UINT
        decode_uint(additional_info)
      when TYPE_NEGINT
        value = decode_uint(additional_info)
        -1 - value
      when TYPE_BYTES
        length = decode_uint(additional_info)
        read_bytes(length)
      when TYPE_TEXT
        length = decode_uint(additional_info)
        read_bytes(length)
      when TYPE_ARRAY
        decode_array(additional_info)
      when TYPE_MAP
        decode_map(additional_info)
      when TYPE_TAG
        tag = decode_uint(additional_info)
        value = decode_value
        Tagged.new(tag, value)
      when TYPE_SIMPLE
        decode_simple(additional_info)
      else
        raise DecodeError.new("Unknown major type: #{major_type}")
      end
    end

    def decode_uint(additional_info)
      case additional_info
      when 0..23
        additional_info
      when AI_UINT8
        read_byte
      when AI_UINT16
        read_bytes(2).unpack("n")[0]
      when AI_UINT32
        read_bytes(4).unpack("N")[0]
      when AI_UINT64
        high, low = read_bytes(8).unpack("NN")
        (high << 32) | low
      else
        raise DecodeError.new("Invalid additional info for uint: #{additional_info}")
      end
    end

    def decode_array(additional_info)
      length = decode_uint(additional_info)
      result = []
      length.times do
        result << decode_value
      end
      result
    end

    def decode_map(additional_info)
      length = decode_uint(additional_info)
      result = {}
      length.times do
        key = decode_value
        value = decode_value
        result[key] = value
      end
      result
    end

    def decode_simple(additional_info)
      case additional_info
      when SIMPLE_FALSE
        false
      when SIMPLE_TRUE
        true
      when SIMPLE_NULL
        nil
      when SIMPLE_UNDEFINED
        nil  # Treat undefined as nil
      when 25
        # float16 (half-precision)
        # Simplified: treat as 0 or read as uint16 and approximate
        read_bytes(2)
        0.0
      when 26
        # float32
        read_bytes(4).unpack("g")[0]
      when 27
        # float64
        read_bytes(8).unpack("G")[0]
      else
        raise DecodeError.new("Unknown simple value: #{additional_info}")
      end
    end

    def read_byte
      raise DecodeError.new("Unexpected end of data") if @offset >= @data.length
      byte = @data[@offset].ord
      @offset += 1
      byte
    end

    def read_bytes(n)
      raise DecodeError.new("Unexpected end of data") if @offset + n > @data.length
      result = @data[@offset, n]
      @offset += n
      result
    end
  end
end
