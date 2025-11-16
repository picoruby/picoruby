#
# MessagePack library for PicoRuby
# This is a pure Ruby implementation of MessagePack serializer/deserializer.
# It is designed to be small and simple, not to be fast or complete.
#
# Author: Hitoshi HASUMI
# License: MIT
#

module MessagePack
  class MessagePackError < StandardError; end
  class PackError < MessagePackError; end
  class UnpackError < MessagePackError; end

  def self.pack(obj)
    Packer.new.pack(obj)
  end

  def self.unpack(data)
    Unpacker.new(data).unpack
  end

  class Packer
    def initialize
      @result = ""
    end

    def pack(obj)
      @result = ""
      pack_value(obj)
      @result
    end

    private

    def pack_value(obj)
      case obj
      when NilClass
        pack_nil
      when TrueClass
        pack_true
      when FalseClass
        pack_false
      when Integer
        pack_integer(obj)
      when Float
        pack_float(obj)
      when String
        pack_string(obj)
      when Array
        pack_array(obj)
      when Hash
        pack_hash(obj)
      else
        raise PackError.new("Unsupported type: #{obj.class}")
      end
    end

    def pack_nil
      @result += [0xc0].pack("C")
    end

    def pack_true
      @result += [0xc3].pack("C")
    end

    def pack_false
      @result += [0xc2].pack("C")
    end

    def pack_integer(val)
      if val >= 0
        if val <= 0x7f
          # positive fixint
          @result += [val].pack("C")
        elsif val <= 0xff
          # uint 8
          @result += [0xcc, val].pack("CC")
        elsif val <= 0xffff
          # uint 16
          @result += [0xcd].pack("C") + [val].pack("n")
        elsif val <= 0xffffffff
          # uint 32
          @result += [0xce].pack("C") + [val].pack("N")
        else
          # uint 64 (not fully supported in 32-bit systems)
          @result += [0xcf].pack("C") + [val >> 32, val & 0xffffffff].pack("NN")
        end
      else
        if val >= -32
          # negative fixint
          @result += [val].pack("c")
        elsif val >= -128
          # int 8
          @result += [0xd0, val].pack("Cc")
        elsif val >= -32768
          # int 16
          @result += [0xd1].pack("C") + [val].pack("s>")
        elsif val >= -2147483648
          # int 32
          @result += [0xd2].pack("C") + [val].pack("l>")
        else
          # int 64
          high = val >> 32
          low = val & 0xffffffff
          @result += [0xd3].pack("C") + [high, low].pack("l>L>")
        end
      end
    end

    def pack_float(val)
      # Use float 64 (double)
      @result += [0xcb].pack("C") + [val].pack("G")
    end

    def pack_string(val)
      len = val.length
      if len <= 31
        # fixstr
        @result += [0xa0 | len].pack("C") + val
      elsif len <= 0xff
        # str 8
        @result += [0xd9, len].pack("CC") + val
      elsif len <= 0xffff
        # str 16
        @result += [0xda].pack("C") + [len].pack("n") + val
      else
        # str 32
        @result += [0xdb].pack("C") + [len].pack("N") + val
      end
    end

    def pack_array(arr)
      len = arr.length
      if len <= 15
        # fixarray
        @result += [0x90 | len].pack("C")
      elsif len <= 0xffff
        # array 16
        @result += [0xdc].pack("C") + [len].pack("n")
      else
        # array 32
        @result += [0xdd].pack("C") + [len].pack("N")
      end
      arr.each do |elem|
        pack_value(elem)
      end
    end

    def pack_hash(hash)
      len = hash.length
      if len <= 15
        # fixmap
        @result += [0x80 | len].pack("C")
      elsif len <= 0xffff
        # map 16
        @result += [0xde].pack("C") + [len].pack("n")
      else
        # map 32
        @result += [0xdf].pack("C") + [len].pack("N")
      end
      hash.each do |key, value|
        pack_value(key)
        pack_value(value)
      end
    end
  end

  class Unpacker
    def initialize(data)
      @data = data
      @index = 0
    end

    def unpack
      unpack_value
    end

    private

    def unpack_value
      byte = read_byte

      if byte <= 0x7f
        # positive fixint
        byte
      elsif byte <= 0x8f
        # fixmap
        unpack_map(byte & 0x0f)
      elsif byte <= 0x9f
        # fixarray
        unpack_array(byte & 0x0f)
      elsif byte <= 0xbf
        # fixstr
        unpack_string(byte & 0x1f)
      elsif byte == 0xc0
        # nil
        nil
      elsif byte == 0xc2
        # false
        false
      elsif byte == 0xc3
        # true
        true
      elsif byte == 0xca
        # float 32
        unpack_float32
      elsif byte == 0xcb
        # float 64
        unpack_float64
      elsif byte == 0xcc
        # uint 8
        read_byte
      elsif byte == 0xcd
        # uint 16
        read_uint16
      elsif byte == 0xce
        # uint 32
        read_uint32
      elsif byte == 0xcf
        # uint 64
        read_uint64
      elsif byte == 0xd0
        # int 8
        read_int8
      elsif byte == 0xd1
        # int 16
        read_int16
      elsif byte == 0xd2
        # int 32
        read_int32
      elsif byte == 0xd3
        # int 64
        read_int64
      elsif byte == 0xd9
        # str 8
        len = read_byte
        unpack_string(len)
      elsif byte == 0xda
        # str 16
        len = read_uint16
        unpack_string(len)
      elsif byte == 0xdb
        # str 32
        len = read_uint32
        unpack_string(len)
      elsif byte == 0xdc
        # array 16
        len = read_uint16
        unpack_array(len)
      elsif byte == 0xdd
        # array 32
        len = read_uint32
        unpack_array(len)
      elsif byte == 0xde
        # map 16
        len = read_uint16
        unpack_map(len)
      elsif byte == 0xdf
        # map 32
        len = read_uint32
        unpack_map(len)
      elsif byte >= 0xe0
        # negative fixint
        byte - 256
      else
        raise UnpackError.new("Unknown byte: 0x#{byte.to_s(16)}")
      end
    end

    def read_byte
      raise UnpackError.new("Unexpected end of data") if @index >= @data.length
      byte = @data[@index].ord
      @index += 1
      byte
    end

    def read_bytes(n)
      raise UnpackError.new("Unexpected end of data") if @index + n > @data.length
      result = @data[@index, n]
      @index += n
      result
    end

    def read_uint16
      read_bytes(2).unpack("n")[0]
    end

    def read_uint32
      read_bytes(4).unpack("N")[0]
    end

    def read_uint64
      high, low = read_bytes(8).unpack("NN")
      (high << 32) | low
    end

    def read_int8
      read_bytes(1).unpack("c")[0]
    end

    def read_int16
      read_bytes(2).unpack("s>")[0]
    end

    def read_int32
      read_bytes(4).unpack("l>")[0]
    end

    def read_int64
      high, low = read_bytes(8).unpack("l>L>")
      (high << 32) | low
    end

    def read_float32
      read_bytes(4).unpack("g")[0]
    end

    def unpack_float32
      read_bytes(4).unpack("g")[0]
    end

    def unpack_float64
      read_bytes(8).unpack("G")[0]
    end

    def unpack_string(len)
      read_bytes(len)
    end

    def unpack_array(len)
      result = []
      len.times do
        result << unpack_value
      end
      result
    end

    def unpack_map(len)
      result = {}
      len.times do
        key = unpack_value
        value = unpack_value
        result[key] = value
      end
      result
    end
  end
end
