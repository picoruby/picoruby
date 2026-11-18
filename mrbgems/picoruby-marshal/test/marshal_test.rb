class MarshalTest < Picotest::Test
  def test_marshal_nil
    data = Marshal.dump(nil)
    result = Marshal.load(data)
    assert_equal nil, result
  end

  def test_marshal_true
    data = Marshal.dump(true)
    result = Marshal.load(data)
    assert_equal true, result
  end

  def test_marshal_false
    data = Marshal.dump(false)
    result = Marshal.load(data)
    assert_equal false, result
  end

  def test_marshal_integer_zero
    data = Marshal.dump(0)
    result = Marshal.load(data)
    assert_equal 0, result
  end

  def test_marshal_integer_positive_small
    data = Marshal.dump(42)
    result = Marshal.load(data)
    assert_equal 42, result
  end

  def test_marshal_integer_negative_small
    data = Marshal.dump(-42)
    result = Marshal.load(data)
    assert_equal(-42, result)
  end

  def test_marshal_integer_256
    data = Marshal.dump(256)
    result = Marshal.load(data)
    assert_equal 256, result
  end

  def test_marshal_integer_large
    data = Marshal.dump(123456)
    result = Marshal.load(data)
    assert_equal 123456, result
  end

  def test_marshal_integer_2byte_utf8_bytes
    # 32960 (0x80C0) encodes as 2-byte LE bytes [0xC0, 0x80], which happen to form
    # a valid UTF-8 2-byte sequence. String#[] in UTF-8 mode reads them as 1 char,
    # causing the position counter to skip the type byte of the next array element.
    data = Marshal.dump([32960, 1])
    result = Marshal.load(data)
    assert_equal [32960, 1], result
  end

  def test_marshal_string_empty
    data = Marshal.dump("")
    result = Marshal.load(data)
    assert_equal "", result
  end

  def test_marshal_string
    data = Marshal.dump("hello")
    result = Marshal.load(data)
    assert_equal "hello", result
  end

  def test_marshal_symbol
    data = Marshal.dump(:test)
    result = Marshal.load(data)
    assert_equal :test, result
  end

  def test_marshal_array_empty
    data = Marshal.dump([])
    result = Marshal.load(data)
    assert_equal [], result
  end

  def test_marshal_array_integers
    data = Marshal.dump([1, 2, 3])
    result = Marshal.load(data)
    assert_equal [1, 2, 3], result
  end

  def test_marshal_array_mixed
    data = Marshal.dump([1, "two", :three])
    result = Marshal.load(data)
    assert_equal [1, "two", :three], result
  end

  def test_marshal_hash_empty
    data = Marshal.dump({})
    result = Marshal.load(data)
    assert_equal({}, result)
  end

  def test_marshal_hash_simple
    data = Marshal.dump({a: 1, b: 2})
    result = Marshal.load(data)
    assert_equal({a: 1, b: 2}, result)
  end

  def test_marshal_nested_array
    data = Marshal.dump([1, [2, 3], 4])
    result = Marshal.load(data)
    assert_equal [1, [2, 3], 4], result
  end

  def test_marshal_nested_hash
    data = Marshal.dump({a: {b: 1}})
    result = Marshal.load(data)
    assert_equal({a: {b: 1}}, result)
  end

  def test_marshal_drb_message_format
    # Simulate dRuby message: [ref, method_id, args, block]
    msg = [123, :call, ["arg1", "arg2"], nil]
    data = Marshal.dump(msg)
    result = Marshal.load(data)
    assert_equal msg, result
  end

  def test_marshal_version_check
    data = "\x04\x08" + "0"  # Valid version + nil
    result = Marshal.load(data)
    assert_equal nil, result
  end

  def test_marshal_invalid_version
    assert_raise(TypeError) do
      Marshal.load("\x05\x08" + "0")  # Wrong major version
    end
  end

  def test_marshal_too_short
    assert_raise(ArgumentError) do
      Marshal.load("\x04")  # Too short
    end
  end

  class Point
    attr_reader :x, :y
    def initialize(x, y)
      @x = x
      @y = y
    end
  end

  class Pair
    attr_reader :a
    def initialize(a)
      @a = a
    end

    def marshal_dump
      [@a, :extra]
    end

    def marshal_load(data)
      @a = data[0]
    end
  end

  class TaggedError < StandardError
    attr_reader :tag
    def initialize(message, tag)
      super(message)
      @tag = tag
    end
  end

  # Objects other than the core values are the mruby VM's: mruby/c has
  # neither Module nor allocate
  def objects?
    Object.const_defined?(:Module) && Point.respond_to?(:allocate)
  end

  def test_marshal_object_dumps_as_class_and_instance_variables
    return unless objects?
    data = Marshal.dump(Point.new(1, "two"))
    point = Marshal.load(data)
    assert_equal Point, point.class
    assert_equal 1, point.x
    assert_equal "two", point.y
  end

  def test_marshal_object_with_marshal_dump
    return unless objects?
    pair = Marshal.load(Marshal.dump(Pair.new([1, 2])))
    assert_equal Pair, pair.class
    assert_equal [1, 2], pair.a
  end

  def test_marshal_exception_keeps_message_backtrace_and_ivars
    return unless objects?
    error = TaggedError.new("broken", :t)
    error.set_backtrace(["a.rb:1"])
    data = Marshal.dump(error)
    loaded = Marshal.load(data)
    assert_equal TaggedError, loaded.class
    assert_equal "broken", loaded.message
    assert_equal ["a.rb:1"], loaded.backtrace
    assert_equal :t, loaded.tag
  end

  def test_marshal_class_and_module
    return unless objects?
    assert_equal Point, Marshal.load(Marshal.dump(Point))
    assert_equal Marshal, Marshal.load(Marshal.dump(Marshal))
  end

  def test_marshal_proc_raises_type_error
    return unless objects?
    assert_raise(TypeError) { Marshal.dump(Proc.new { }) }
  end

  def test_marshal_object_referring_to_itself_raises
    return unless objects?
    point = Point.new(1, nil)
    point.instance_variable_set(:@y, point)
    assert_raise(ArgumentError) { Marshal.dump(point) }
    assert_equal 1, Marshal.load(Marshal.dump(Point.new(1, 2))).x
  end

  # CRuby writes a symbol in full once and as ';' and its index after that
  def test_marshal_load_symlink
    assert_equal [:a, :a], Marshal.load("\x04\x08[\x07:\x06a;\x00")
    assert_raise(ArgumentError) { Marshal.load("\x04\x08[\x07:\x06a;\x06") }
  end

  def test_marshal_load_objects_dumped_by_cruby
    return unless objects?
    data = "\x04\x08[\x07o:\x17MarshalTest::Point\x07:\x07@xi\x06:\x07@yi\x07o;\x00\x07;\x06i\x08;\x07i\x09"
    points = Marshal.load(data)
    assert_equal MarshalTest::Point, points[1].class
    assert_equal 3, points[1].x
    assert_equal 4, points[1].y
  end

  def test_marshal_built_in_without_ivar_state_raises_type_error
    return unless objects?
    assert_raise(TypeError) { Marshal.dump(1..3) }
  end

  def test_marshal_load_class_checks_kind
    return unless objects?
    assert_raise(ArgumentError) { Marshal.load("\x04\x08c\x0cMarshal") }
    assert_raise(ArgumentError) { Marshal.load("\x04\x08m\x0bObject") }
  end

  def test_marshal_object_raises_argument_error_without_objects
    return if objects?
    assert_raise(ArgumentError) { Marshal.dump(Point.new(1, 2)) }
  end
end
