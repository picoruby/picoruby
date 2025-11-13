require 'pack'

module Marshal
  MAJOR_VERSION = 4
  MINOR_VERSION = 8
  VERSION_STRING = "\x04\x08"

  TYPE_NIL       = '0'
  TYPE_TRUE      = 'T'
  TYPE_FALSE     = 'F'
  TYPE_FIXNUM    = 'i'
  TYPE_STRING    = '"'
  TYPE_SYMBOL    = ':'
  TYPE_ARRAY     = '['
  TYPE_HASH      = '{'

  class << self
    def dump(obj)
      result = VERSION_STRING.dup
      result << dump_object(obj)
      result
    end

    def load(data)
      raise ArgumentError, "marshal data too short" if data.size < 2

      major = data[0].ord
      minor = data[1].ord

      if major != MAJOR_VERSION || minor != MINOR_VERSION
        raise TypeError, "incompatible marshal file format (can't be read)\n\tformat version #{major}.#{minor} required; #{MAJOR_VERSION}.#{MINOR_VERSION} given"
      end

      pos = 2
      obj, pos = load_object(data, pos)
      obj
    end

    private

    def dump_object(obj)
      case obj
      when NilClass
        TYPE_NIL
      when TrueClass
        TYPE_TRUE
      when FalseClass
        TYPE_FALSE
      when Integer
        dump_integer(obj)
      when String
        dump_string(obj)
      when Symbol
        dump_symbol(obj)
      when Array
        dump_array(obj)
      when Hash
        dump_hash(obj)
      else
        raise ArgumentError, "can't dump #{obj.class}"
      end
    end

    def dump_integer(n)
      TYPE_FIXNUM + encode_fixnum(n)
    end

    def encode_fixnum(n)
      if n == 0
        "\x00"
      elsif n > 0
        if n < 123
          [n + 5].pack('C')
        elsif n < 256
          "\x01" + [n].pack('C')
        elsif n < 65536
          "\x02" + [n].pack('v')
        elsif n < 16777216
          "\x03" + [n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF].pack('CCC')
        else
          "\x04" + [n].pack('V')
        end
      else
        # Negative numbers
        if n >= -123
          [n - 5].pack('c')
        elsif n >= -256
          "\xFF" + [n].pack('c')
        elsif n >= -65536
          "\xFE" + [n].pack('v')
        elsif n >= -16777216
          "\xFD" + [n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF].pack('CCC')
        else
          "\xFC" + [n].pack('V')
        end
      end
    end

    def dump_string(s)
      TYPE_STRING + encode_fixnum(s.size) + s
    end

    def dump_symbol(sym)
      name = sym.to_s
      TYPE_SYMBOL + encode_fixnum(name.size) + name
    end

    def dump_array(ary)
      result = TYPE_ARRAY + encode_fixnum(ary.size)
      ary.each do |elem|
        result << dump_object(elem)
      end
      result
    end

    def dump_hash(hash)
      result = TYPE_HASH + encode_fixnum(hash.size)
      hash.each do |key, value|
        result << dump_object(key)
        result << dump_object(value)
      end
      result
    end

    def load_object(data, pos)
      raise ArgumentError, "marshal data too short" if pos >= data.size

      type = data[pos]
      pos += 1

      case type
      when TYPE_NIL
        [nil, pos]
      when TYPE_TRUE
        [true, pos]
      when TYPE_FALSE
        [false, pos]
      when TYPE_FIXNUM
        load_integer(data, pos)
      when TYPE_STRING
        load_string(data, pos)
      when TYPE_SYMBOL
        load_symbol(data, pos)
      when TYPE_ARRAY
        load_array(data, pos)
      when TYPE_HASH
        load_hash(data, pos)
      else
        raise ArgumentError, "unsupported type: #{type.ord.to_s(16)}"
      end
    end

    def load_integer(data, pos)
      n, pos = decode_fixnum(data, pos)
      [n, pos]
    end

    def decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos >= data.size

      c = data[pos].ord
      pos += 1

      if c == 0
        return [0, pos]
      elsif c > 5 && c < 128
        return [c - 5, pos]
      elsif c >= 128 && c < 251
        return [c - 251, pos]
      elsif c == 1
        raise ArgumentError, "marshal data too short" if pos >= data.size
        n = data[pos].ord
        return [n, pos + 1]
      elsif c == 2
        raise ArgumentError, "marshal data too short" if pos + 1 >= data.size
        n = data[pos, 2].unpack('v')[0]
        return [n, pos + 2]
      elsif c == 3
        raise ArgumentError, "marshal data too short" if pos + 2 >= data.size
        bytes = data[pos, 3].unpack('CCC')
        n = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16)
        return [n, pos + 3]
      elsif c == 4
        raise ArgumentError, "marshal data too short" if pos + 3 >= data.size
        n = data[pos, 4].unpack('V')[0]
        return [n, pos + 4]
      elsif c == 255
        raise ArgumentError, "marshal data too short" if pos >= data.size
        n = data[pos].unpack('c')[0]
        return [n, pos + 1]
      elsif c == 254
        raise ArgumentError, "marshal data too short" if pos + 1 >= data.size
        n = data[pos, 2].unpack('v')[0]
        # Convert to signed
        n = n - 65536 if n > 32767
        return [n, pos + 2]
      elsif c == 253
        raise ArgumentError, "marshal data too short" if pos + 2 >= data.size
        bytes = data[pos, 3].unpack('CCC')
        n = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16)
        # Convert to signed 24-bit
        n = n - 16777216 if n > 8388607
        return [n, pos + 3]
      elsif c == 252
        raise ArgumentError, "marshal data too short" if pos + 3 >= data.size
        n = data[pos, 4].unpack('V')[0]
        # Convert to signed 32-bit
        n = n - 4294967296 if n > 2147483647
        return [n, pos + 4]
      else
        raise ArgumentError, "unknown fixnum encoding: #{c.to_s(16)}"
      end
    end

    def load_string(data, pos)
      len, pos = decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos + len > data.size
      str = data[pos, len]
      [str, pos + len]
    end

    def load_symbol(data, pos)
      len, pos = decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos + len > data.size
      name = data[pos, len]
      [name.to_sym, pos + len]
    end

    def load_array(data, pos)
      len, pos = decode_fixnum(data, pos)
      ary = []
      len.times do
        elem, pos = load_object(data, pos)
        ary << elem
      end
      [ary, pos]
    end

    def load_hash(data, pos)
      len, pos = decode_fixnum(data, pos)
      hash = {}
      len.times do
        key, pos = load_object(data, pos)
        value, pos = load_object(data, pos)
        hash[key] = value
      end
      [hash, pos]
    end
  end
end
