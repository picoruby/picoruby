class MessagePackTest < Picotest::Test
  def test_pack_unpack_nil
    packed = MessagePack.pack(nil)
    assert_equal(nil, MessagePack.unpack(packed))
  end

  def test_pack_unpack_true
    packed = MessagePack.pack(true)
    assert_equal(true, MessagePack.unpack(packed))
  end

  def test_pack_unpack_false
    packed = MessagePack.pack(false)
    assert_equal(false, MessagePack.unpack(packed))
  end

  def test_pack_unpack_positive_fixint
    packed = MessagePack.pack(42)
    assert_equal(42, MessagePack.unpack(packed))
  end

  def test_pack_unpack_negative_fixint
    packed = MessagePack.pack(-1)
    assert_equal(-1, MessagePack.unpack(packed))
  end

  def test_pack_unpack_uint8
    packed = MessagePack.pack(200)
    assert_equal(200, MessagePack.unpack(packed))
  end

  def test_pack_unpack_uint16
    packed = MessagePack.pack(30000)
    assert_equal(30000, MessagePack.unpack(packed))
  end

  def test_pack_unpack_uint32
    packed = MessagePack.pack(100000)
    assert_equal(100000, MessagePack.unpack(packed))
  end

  def test_pack_unpack_int8
    packed = MessagePack.pack(-100)
    assert_equal(-100, MessagePack.unpack(packed))
  end

  def test_pack_unpack_int16
    packed = MessagePack.pack(-30000)
    assert_equal(-30000, MessagePack.unpack(packed))
  end

  def test_pack_unpack_int32
    packed = MessagePack.pack(-100000)
    assert_equal(-100000, MessagePack.unpack(packed))
  end

  def test_pack_unpack_float
    packed = MessagePack.pack(3.14)
    unpacked = MessagePack.unpack(packed)
    # Float comparison with small delta
    assert((unpacked - 3.14).abs < 0.001)
  end

  def test_pack_unpack_fixstr
    packed = MessagePack.pack("hello")
    assert_equal("hello", MessagePack.unpack(packed))
  end

  def test_pack_unpack_str8
    # String longer than 31 bytes
    str = "a" * 50
    packed = MessagePack.pack(str)
    assert_equal(str, MessagePack.unpack(packed))
  end

  def test_pack_unpack_str16
    # String longer than 255 bytes
    str = "b" * 300
    packed = MessagePack.pack(str)
    assert_equal(str, MessagePack.unpack(packed))
  end

  def test_pack_unpack_fixarray
    packed = MessagePack.pack([1, 2, 3])
    assert_equal([1, 2, 3], MessagePack.unpack(packed))
  end

  def test_pack_unpack_array16
    # Array with more than 15 elements
    arr = []
    20.times do |i|
      arr << i
    end
    packed = MessagePack.pack(arr)
    assert_equal(arr, MessagePack.unpack(packed))
  end

  def test_pack_unpack_fixmap
    packed = MessagePack.pack({"name" => "Alice", "age" => 30})
    result = MessagePack.unpack(packed)
    assert_equal("Alice", result["name"])
    assert_equal(30, result["age"])
  end

  def test_pack_unpack_nested_structure
    data = {
      "name" => "Bob",
      "age" => 25,
      "scores" => [85, 90, 95],
      "address" => {
        "city" => "Tokyo",
        "zip" => "100-0001"
      }
    }
    packed = MessagePack.pack(data)
    result = MessagePack.unpack(packed)
    assert_equal("Bob", result["name"])
    assert_equal(25, result["age"])
    assert_equal([85, 90, 95], result["scores"])
    assert_equal("Tokyo", result["address"]["city"])
    assert_equal("100-0001", result["address"]["zip"])
  end

  def test_pack_unpack_mixed_types
    data = [1, "two", true, nil, 3.14, {"key" => "value"}]
    packed = MessagePack.pack(data)
    result = MessagePack.unpack(packed)
    assert_equal(1, result[0])
    assert_equal("two", result[1])
    assert_equal(true, result[2])
    assert_equal(nil, result[3])
    assert((result[4] - 3.14).abs < 0.001)
    assert_equal("value", result[5]["key"])
  end

  def test_pack_unpack_empty_array
    packed = MessagePack.pack([])
    assert_equal([], MessagePack.unpack(packed))
  end

  def test_pack_unpack_empty_hash
    packed = MessagePack.pack({})
    assert_equal({}, MessagePack.unpack(packed))
  end

  def test_pack_unpack_empty_string
    packed = MessagePack.pack("")
    assert_equal("", MessagePack.unpack(packed))
  end

  def test_pack_unpack_zero
    packed = MessagePack.pack(0)
    assert_equal(0, MessagePack.unpack(packed))
  end

  def test_pack_unpack_symbol_keys
    # Symbols should be converted to strings
    packed = MessagePack.pack({name: "Charlie", age: 35})
    result = MessagePack.unpack(packed)
    assert_equal("Charlie", result[:name])
    assert_equal(35, result[:age])
  end
end
