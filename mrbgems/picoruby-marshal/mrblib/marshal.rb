require 'pack'

module Marshal
  MAJOR_VERSION = 4
  MINOR_VERSION = 8
  VERSION_STRING = "\x04\x08"

  TYPE_NIL       = '0'
  TYPE_TRUE      = 'T'
  TYPE_FALSE     = 'F'
  TYPE_FIXNUM    = 'i'
  TYPE_FLOAT     = 'f'
  TYPE_STRING    = '"'
  TYPE_SYMBOL    = ':'
  TYPE_SYMLINK   = ';'
  TYPE_ARRAY     = '['
  TYPE_HASH      = '{'
  TYPE_IVAR      = 'I'
  TYPE_OBJECT    = 'o'
  TYPE_USERDEF   = 'u'
  TYPE_USRMARSHAL = 'U'
  TYPE_CLASS     = 'c'
  TYPE_MODULE    = 'm'

  # Built-in classes that keep their state outside instance variables: as
  # an 'o' record they would load back empty, so dump raises instead
  NO_IVAR_STATE = %w[Range Time Struct Data Method UnboundMethod Rational Complex Random]

  class << self
    def dump(obj) # steep:ignore MethodArityMismatch
      result = VERSION_STRING.dup
      result << dump_object(obj)
      result
    end

    # Do not load data from an untrusted source: the data names the classes
    # to allocate and the instance variables to set, and calls their
    # marshal_load or _load, as in CRuby.
    def load(data) # steep:ignore MethodArityMismatch
      # @type var data: String
      raise ArgumentError, "marshal data too short" if data.bytesize < 2

      major = data.getbyte(0) || 0
      minor = data.getbyte(1) || 0

      if major != MAJOR_VERSION || minor != MINOR_VERSION
        raise TypeError, "incompatible marshal file format (can't be read)\n\tformat version #{major}.#{minor} required; #{MAJOR_VERSION}.#{MINOR_VERSION} given"
      end

      pos = 2
      # the symbols read so far, for ';' to refer to
      symbols = [] #: Array[Symbol]
      obj, pos = load_object(data, pos, symbols)
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
        # @type var obj: Integer
        dump_integer(obj)
      when Float
        # @type var obj: Float
        dump_float(obj)
      when String
        # @type var obj: String
        dump_string(obj)
      when Symbol
        # @type var obj: Symbol
        dump_symbol(obj)
      when Array
        dump_array(obj)
      when Hash
        dump_hash(obj)
      else
        dump_other(obj)
      end
    end

    # Classes, modules and other objects, on the mruby VM. mruby/c has no
    # Class or Module to tell them by, nor Exception#backtrace.
    def dump_other(obj)
      raise ArgumentError, "can't dump #{obj.class}" unless Object.const_defined?(:Module)
      if obj.is_a?(Class)
        TYPE_CLASS + dump_bytes(class_path(obj))
      elsif obj.is_a?(Module)
        TYPE_MODULE + dump_bytes(class_path(obj))
      elsif obj.is_a?(Proc)
        raise TypeError, "no _dump_data is defined for class #{obj.class}"
      else
        if obj.respond_to?(:marshal_dump)
          TYPE_USRMARSHAL + dump_symbol(class_path(obj.class).to_sym) +
            dump_nested(obj) { dump_object(obj.marshal_dump) }
        elsif obj.respond_to?(:_dump)
          TYPE_USERDEF + dump_symbol(class_path(obj.class).to_sym) +
            dump_bytes(obj._dump(-1))
        elsif no_ivar_state?(obj)
          raise TypeError, "can't dump #{obj.class}"
        else
          dump_nested(obj) { dump_plain_object(obj) }
        end
      end
    end

    # The name a class or module is loaded back by
    def class_path(mod)
      name = mod.respond_to?(:name) ? mod.name : mod.to_s
      if name.nil? || name.start_with?("#")
        raise TypeError, "can't dump anonymous #{mod.is_a?(Class) ? "class" : "module"} #{mod}"
      end
      name
    end

    def dump_bytes(str)
      encode_fixnum(str.bytesize) + str
    end

    def no_ivar_state?(obj)
      i = 0
      while i < NO_IVAR_STATE.size
        name = NO_IVAR_STATE[i]
        return true if Object.const_defined?(name) && obj.is_a?(Object.const_get(name))
        i += 1
      end
      false
    end

    # An object refers to itself, or to one being dumped around it: there
    # are no object links here, so dumping would not end.
    def dump_nested(obj)
      stack = (@dumping ||= [])
      id = obj.object_id
      i = 0
      while i < stack.size
        raise ArgumentError, "can't dump an object that refers to itself (#{obj.class})" if stack[i] == id
        i += 1
      end
      stack << id
      begin
        result = yield
      ensure
        stack.pop
      end
      result
    end

    # An object as its class and instance variables, @ and all. An exception
    # has its message and backtrace first, as mesg and bt with no @, as
    # CRuby has them.
    def dump_plain_object(obj)
      ivars = obj.instance_variables
      body = ""
      count = ivars.size
      if obj.is_a?(Exception)
        body << dump_symbol(:mesg) << dump_object(obj.message)
        body << dump_symbol(:bt) << dump_object(obj.backtrace)
        count += 2
      end
      i = 0
      while i < ivars.size
        name = ivars[i]
        body << dump_symbol(name)
        body << dump_object(obj.instance_variable_get(name))
        i += 1
      end
      TYPE_OBJECT + dump_symbol(class_path(obj.class).to_sym) + encode_fixnum(count) + body
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

    def dump_float(f)
      str = f.to_s
      TYPE_FLOAT + encode_fixnum(str.size) + str
    end

    def dump_string(s)
      TYPE_STRING + encode_fixnum(s.bytesize) + s
    end

    def dump_symbol(sym)
      name = sym.to_s
      TYPE_SYMBOL + encode_fixnum(name.bytesize) + name
    end

    def dump_array(ary)
      result = TYPE_ARRAY + encode_fixnum(ary.size)
      i = 0
      while i < ary.size
        result << dump_object(ary[i])
        i += 1
      end
      result
    end

    def dump_hash(hash)
      result = TYPE_HASH + encode_fixnum(hash.size)
      keys = hash.keys
      i = 0
      while i < keys.size
        key = keys[i]
        result << dump_object(key)
        result << dump_object(hash[key])
        i += 1
      end
      result
    end

    def load_object(data, pos, symbols)
      raise ArgumentError, "marshal data too short" if pos >= data.bytesize

      type = [(data.getbyte(pos) || 0)].pack("C")
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
      when TYPE_FLOAT
        load_float(data, pos)
      when TYPE_STRING
        load_string(data, pos)
      when TYPE_SYMBOL
        load_symbol(data, pos, symbols)
      when TYPE_SYMLINK
        load_symlink(data, pos, symbols)
      when TYPE_ARRAY
        load_array(data, pos, symbols)
      when TYPE_HASH
        load_hash(data, pos, symbols)
      when TYPE_IVAR
        load_ivar(data, pos, symbols)
      when TYPE_OBJECT
        load_plain_object(data, pos, symbols)
      when TYPE_USRMARSHAL
        name, pos = load_typed_symbol(data, pos, symbols)
        obj = allocate(path_to_class(name))
        value, pos = load_object(data, pos, symbols)
        obj.marshal_load(value)
        [obj, pos]
      when TYPE_USERDEF
        name, pos = load_typed_symbol(data, pos, symbols)
        str, pos = load_string(data, pos)
        [path_to_class(name)._load(str), pos]
      when TYPE_CLASS, TYPE_MODULE
        load_class_or_module(data, pos, type)
      else
        raise ArgumentError, "unsupported type: #{(type.getbyte(0) || 0).to_s(16)}"
      end
    end

    def load_integer(data, pos)
      n, pos = decode_fixnum(data, pos)
      [n, pos]
    end

    def decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos >= data.bytesize

      c = data.getbyte(pos) || 0
      pos += 1

      if c == 0
        return [0, pos]
      elsif c > 5 && c < 128
        return [c - 5, pos]
      elsif c >= 128 && c < 251
        return [c - 251, pos]
      elsif c == 1
        raise ArgumentError, "marshal data too short" if pos >= data.bytesize
        n = data.getbyte(pos) || 0
        return [n, pos + 1]
      elsif c == 2
        raise ArgumentError, "marshal data too short" if pos + 1 >= data.bytesize
        n = (data.getbyte(pos) || 0) | ((data.getbyte(pos + 1) || 0) << 8)
        return [n, pos + 2]
      elsif c == 3
        raise ArgumentError, "marshal data too short" if pos + 2 >= data.bytesize
        n = (data.getbyte(pos) || 0) |
            ((data.getbyte(pos + 1) || 0) << 8) |
            ((data.getbyte(pos + 2) || 0) << 16)
        return [n, pos + 3]
      elsif c == 4
        raise ArgumentError, "marshal data too short" if pos + 3 >= data.bytesize
        n = (data.getbyte(pos) || 0) |
            ((data.getbyte(pos + 1) || 0) << 8) |
            ((data.getbyte(pos + 2) || 0) << 16) |
            ((data.getbyte(pos + 3) || 0) << 24)
        return [n, pos + 4]
      elsif c == 255
        raise ArgumentError, "marshal data too short" if pos >= data.bytesize
        n = data.getbyte(pos) || 0
        n -= 256 if n > 127
        return [n, pos + 1]
      elsif c == 254
        raise ArgumentError, "marshal data too short" if pos + 1 >= data.bytesize
        n = (data.getbyte(pos) || 0) | ((data.getbyte(pos + 1) || 0) << 8)
        n -= 65536 if n > 32767
        return [n, pos + 2]
      elsif c == 253
        raise ArgumentError, "marshal data too short" if pos + 2 >= data.bytesize
        n = (data.getbyte(pos) || 0) |
            ((data.getbyte(pos + 1) || 0) << 8) |
            ((data.getbyte(pos + 2) || 0) << 16)
        n -= 16777216 if n > 8388607
        return [n, pos + 3]
      elsif c == 252
        raise ArgumentError, "marshal data too short" if pos + 3 >= data.bytesize
        n = (data.getbyte(pos) || 0) |
            ((data.getbyte(pos + 1) || 0) << 8) |
            ((data.getbyte(pos + 2) || 0) << 16) |
            ((data.getbyte(pos + 3) || 0) << 24)
        n -= 4294967296 if n > 2147483647
        return [n, pos + 4]
      else
        raise ArgumentError, "unknown fixnum encoding: #{c.to_s(16)}"
      end
    end

    def load_float(data, pos)
      len, pos = decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos + len > data.size
      str = data[pos, len]
      raise ArgumentError, "marshal data too short" if str.nil?
      [str.to_f, pos + len]
    end

    def load_string(data, pos)
      len, pos = decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos + len > data.bytesize
      [data.byteslice(pos, len) || "", pos + len]
    end

    def load_symbol(data, pos, symbols)
      len, pos = decode_fixnum(data, pos)
      raise ArgumentError, "marshal data too short" if pos + len > data.bytesize
      sym = (data.byteslice(pos, len) || "").to_sym
      symbols << sym
      [sym, pos + len]
    end

    # ';' and the index of a symbol read before: CRuby writes each symbol
    # in full once, and by its index after that
    def load_symlink(data, pos, symbols)
      index, pos = decode_fixnum(data, pos)
      raise ArgumentError, "bad symbol" if index < 0 || symbols.size <= index
      [symbols[index], pos]
    end

    def load_array(data, pos, symbols)
      len, pos = decode_fixnum(data, pos)
      ary = [] #: Array[untyped]
      i = 0
      while i < len
        elem, pos = load_object(data, pos, symbols)
        ary << elem
        i += 1
      end
      [ary, pos]
    end

    def load_hash(data, pos, symbols)
      len, pos = decode_fixnum(data, pos)
      hash = {} #: Hash[untyped, untyped]
      i = 0
      while i < len
        key, pos = load_object(data, pos, symbols)
        value, pos = load_object(data, pos, symbols)
        hash[key] = value
        i += 1
      end
      [hash, pos]
    end

    def load_typed_symbol(data, pos, symbols)
      sym, pos = load_object(data, pos, symbols)
      raise ArgumentError, "dump format error (symbol expected)" unless sym.is_a?(Symbol)
      [sym.to_s, pos]
    end

    def load_class_or_module(data, pos, type)
      name, pos = load_string(data, pos)
      unless Object.const_defined?(:Module)
        raise ArgumentError, "can't load #{name}: no Module on this VM"
      end
      mod = path_to_class(name)
      if type == TYPE_CLASS
        raise ArgumentError, "#{name} does not refer to class" unless mod.is_a?(Class)
      elsif !mod.is_a?(Module) || mod.is_a?(Class)
        raise ArgumentError, "#{name} does not refer to module"
      end
      [mod, pos]
    end

    def path_to_class(name)
      parts = name.split("::")
      mod = Object
      i = 0
      while i < parts.size
        part = parts[i]
        unless mod.const_defined?(part)
          raise ArgumentError, "undefined class/module #{name}"
        end
        mod = mod.const_get(part)
        i += 1
      end
      mod
    end

    def allocate(klass)
      unless klass.respond_to?(:allocate)
        raise TypeError, "can't load #{klass}: no allocate on this VM"
      end
      klass.allocate
    end

    def load_plain_object(data, pos, symbols)
      name, pos = load_typed_symbol(data, pos, symbols)
      obj = allocate(path_to_class(name))
      count, pos = decode_fixnum(data, pos)
      i = 0
      while i < count
        ivar, pos = load_object(data, pos, symbols)
        value, pos = load_object(data, pos, symbols)
        if obj.is_a?(Exception) && ivar == :mesg
          obj = obj.exception(value) if value
        elsif obj.is_a?(Exception) && ivar == :bt
          obj.set_backtrace(value) if value
        elsif ivar.to_s.start_with?("@")
          obj.instance_variable_set(ivar, value)
        end
        i += 1
      end
      [obj, pos]
    end

    def load_ivar(data, pos, symbols)
      # Load the object with instance variables
      obj, pos = load_object(data, pos, symbols)

      # Load number of instance variables
      num_ivars, pos = decode_fixnum(data, pos)

      # Skip instance variables (we just return the base object)
      i = 0
      while i < num_ivars
        # Skip ivar name (symbol)
        _, pos = load_object(data, pos, symbols)
        # Skip ivar value
        _, pos = load_object(data, pos, symbols)
        i += 1
      end

      [obj, pos]
    end
  end
end
