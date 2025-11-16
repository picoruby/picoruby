class CBORTest < Picotest::Test
  def test_encode_decode_unsigned_integer_small
    original = 10
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_unsigned_integer_zero
    original = 0
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_unsigned_integer_23
    original = 23
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_unsigned_integer_uint8
    original = 100
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_unsigned_integer_uint16
    original = 1000
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_unsigned_integer_uint32
    original = 100000
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_negative_integer_small
    original = -1
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_negative_integer_large
    original = -100
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_negative_integer_very_large
    original = -10000
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_float
    original = 3.14
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert((decoded - original).abs < 0.001)
  end

  def test_encode_decode_float_negative
    original = -2.5
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert((decoded - original).abs < 0.001)
  end

  def test_encode_decode_string_empty
    original = ""
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_string_short
    original = "hello"
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_string_long
    original = "The quick brown fox jumps over the lazy dog"
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_string_with_spaces
    original = "Hello, World!"
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_true
    original = true
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_false
    original = false
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_nil
    original = nil
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_array_empty
    original = []
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_array_integers
    original = [1, 2, 3, 4, 5]
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_array_mixed
    original = [1, "two", 3.0, true, false, nil]
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(1, decoded[0])
    assert_equal("two", decoded[1])
    assert((decoded[2] - 3.0).abs < 0.001)
    assert_equal(true, decoded[3])
    assert_equal(false, decoded[4])
    assert_equal(nil, decoded[5])
  end

  def test_encode_decode_array_nested
    original = [1, [2, 3], [4, [5, 6]]]
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(1, decoded[0])
    assert_equal([2, 3], decoded[1])
    assert_equal(4, decoded[2][0])
    assert_equal([5, 6], decoded[2][1])
  end

  def test_encode_decode_map_empty
    original = {}
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_decode_map_simple
    original = {"a" => 1, "b" => 2}
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(1, decoded["a"])
    assert_equal(2, decoded["b"])
  end

  def test_encode_decode_map_mixed_values
    original = {"name" => "test", "count" => 42, "active" => true}
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal("test", decoded["name"])
    assert_equal(42, decoded["count"])
    assert_equal(true, decoded["active"])
  end

  def test_encode_decode_map_nested
    original = {
      "user" => {
        "name" => "Alice",
        "age" => 30
      },
      "items" => [1, 2, 3]
    }
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal("Alice", decoded["user"]["name"])
    assert_equal(30, decoded["user"]["age"])
    assert_equal([1, 2, 3], decoded["items"])
  end

  def test_encode_decode_tagged_value
    tagged = CBOR::Tagged.new(1, 1234567890)  # Tag 1 = epoch timestamp
    encoded = CBOR.encode(tagged)
    decoded = CBOR.decode(encoded)
    assert(decoded.is_a?(CBOR::Tagged))
    assert_equal(1, decoded.tag)
    assert_equal(1234567890, decoded.value)
  end

  def test_encode_decode_tagged_with_string
    tagged = CBOR::Tagged.new(100, "custom data")
    encoded = CBOR.encode(tagged)
    decoded = CBOR.decode(encoded)
    assert(decoded.is_a?(CBOR::Tagged))
    assert_equal(100, decoded.tag)
    assert_equal("custom data", decoded.value)
  end

  def test_encode_complex_structure
    original = {
      "sensor" => "temperature",
      "location" => "room1",
      "values" => [20.5, 21.0, 19.8, 20.2],
      "metadata" => {
        "unit" => "celsius",
        "precision" => 1
      },
      "active" => true
    }
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)

    assert_equal("temperature", decoded["sensor"])
    assert_equal("room1", decoded["location"])
    assert_equal(4, decoded["values"].length)
    assert_equal("celsius", decoded["metadata"]["unit"])
    assert_equal(1, decoded["metadata"]["precision"])
    assert_equal(true, decoded["active"])
  end

  def test_encode_array_of_maps
    original = [
      {"id" => 1, "name" => "Alice"},
      {"id" => 2, "name" => "Bob"}
    ]
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal(2, decoded.length)
    assert_equal(1, decoded[0]["id"])
    assert_equal("Alice", decoded[0]["name"])
    assert_equal(2, decoded[1]["id"])
    assert_equal("Bob", decoded[1]["name"])
  end

  def test_encode_integer_boundaries
    # Test various integer boundaries
    values = [0, 23, 24, 255, 256, 65535, 65536]
    values.each do |val|
      encoded = CBOR.encode(val)
      decoded = CBOR.decode(encoded)
      assert_equal(val, decoded)
    end
  end

  def test_encode_negative_integer_boundaries
    values = [-1, -24, -25, -256, -257]
    values.each do |val|
      encoded = CBOR.encode(val)
      decoded = CBOR.decode(encoded)
      assert_equal(val, decoded)
    end
  end

  def test_encode_symbol_key_map
    original = {name: "test", count: 5}
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal("test", decoded[:name])
    assert_equal(5, decoded[:count])
  end

  def test_encode_integer_as_map_key
    original = {1 => "one", 2 => "two"}
    encoded = CBOR.encode(original)
    decoded = CBOR.decode(encoded)
    assert_equal("one", decoded[1])
    assert_equal("two", decoded[2])
  end

  def test_tagged_equality
    tag1 = CBOR::Tagged.new(1, 100)
    tag2 = CBOR::Tagged.new(1, 100)
    tag3 = CBOR::Tagged.new(2, 100)

    assert(tag1 == tag2)
    assert(!(tag1 == tag3))
  end

  def test_encode_multiple_types
    data = [
      42,
      -17,
      3.14,
      "text",
      true,
      false,
      nil,
      [1, 2, 3],
      {"key" => "value"}
    ]

    data.each do |item|
      encoded = CBOR.encode(item)
      decoded = CBOR.decode(encoded)
      if item.is_a?(Float)
        assert((decoded - item).abs < 0.001)
      else
        # For other types, check structure
        assert(decoded != nil)
      end
    end
  end

  def test_roundtrip_iot_sensor_data
    # Realistic IoT sensor data
    sensor_data = {
      "device_id" => "sensor-001",
      "timestamp" => 1609459200,
      "readings" => [
        {"type" => "temperature", "value" => 22.5, "unit" => "C"},
        {"type" => "humidity", "value" => 65.0, "unit" => "%"},
        {"type" => "pressure", "value" => 1013.25, "unit" => "hPa"}
      ],
      "battery" => 87,
      "online" => true
    }

    encoded = CBOR.encode(sensor_data)
    decoded = CBOR.decode(encoded)

    assert_equal("sensor-001", decoded["device_id"])
    assert_equal(1609459200, decoded["timestamp"])
    assert_equal(3, decoded["readings"].length)
    assert_equal("temperature", decoded["readings"][0]["type"])
    assert_equal(87, decoded["battery"])
    assert_equal(true, decoded["online"])
  end
end
