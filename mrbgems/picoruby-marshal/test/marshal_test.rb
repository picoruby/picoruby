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
end
