class Base58Test < Picotest::Test
  def test_encode_simple
    encoded = Base58.encode("hello")
    assert(encoded.length > 0)
  end

  def test_encode_decode_roundtrip
    original = "Hello, World!"
    encoded = Base58.encode(original)
    decoded = Base58.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_empty_string
    encoded = Base58.encode("")
    assert_equal("", encoded)
  end

  def test_decode_empty_string
    decoded = Base58.decode("")
    assert_equal("", decoded)
  end

  def test_encode_single_byte
    original = "a"
    encoded = Base58.encode(original)
    decoded = Base58.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_leading_zeros
    original = "\x00\x00hello"
    encoded = Base58.encode(original)
    decoded = Base58.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_all_zeros
    original = "\x00\x00\x00"
    encoded = Base58.encode(original)
    decoded = Base58.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_bitcoin_alphabet
    original = "test"
    encoded = Base58.encode(original, alphabet: :bitcoin)
    decoded = Base58.decode(encoded, alphabet: :bitcoin)
    assert_equal(original, decoded)
  end

  def test_encode_flickr_alphabet
    original = "test"
    encoded = Base58.encode(original, alphabet: :flickr)
    decoded = Base58.decode(encoded, alphabet: :flickr)
    assert_equal(original, decoded)
  end

  def test_decode_invalid_character
    assert_raise(Base58::DecodeError) do
      Base58.decode("Invalid0OIl")
    end
  end

  def test_encode_binary_data
    original = "\x01\x02\x03\x04\x05"
    encoded = Base58.encode(original)
    decoded = Base58.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_long_string
    original = "The quick brown fox jumps over the lazy dog"
    encoded = Base58.encode(original)
    decoded = Base58.decode(encoded)
    assert_equal(original, decoded)
  end
end

class Base62Test < Picotest::Test
  def test_encode_simple
    encoded = Base62.encode("hello")
    assert(encoded.length > 0)
  end

  def test_encode_decode_roundtrip
    original = "Hello, World!"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_empty_string
    encoded = Base62.encode("")
    assert_equal("", encoded)
  end

  def test_decode_empty_string
    decoded = Base62.decode("")
    assert_equal("", decoded)
  end

  def test_encode_single_byte
    original = "a"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_leading_zeros
    original = "\x00\x00hello"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_all_zeros
    original = "\x00\x00\x00"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_decode_invalid_character
    assert_raise(Base62::DecodeError) do
      Base62.decode("Invalid@#$")
    end
  end

  def test_encode_binary_data
    original = "\x01\x02\x03\x04\x05"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_long_string
    original = "The quick brown fox jumps over the lazy dog"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_numbers
    original = "1234567890"
    encoded = Base62.encode(original)
    decoded = Base62.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_alphabet_contains_all_chars
    # Test that alphabet contains 0-9, A-Z, a-z
    assert_equal(62, Base62::ALPHABET.length)
  end
end

class Base45Test < Picotest::Test
  def test_encode_simple
    encoded = Base45.encode("hello")
    assert(encoded.length > 0)
  end

  def test_encode_decode_roundtrip
    original = "Hello, World!"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_empty_string
    encoded = Base45.encode("")
    assert_equal("", encoded)
  end

  def test_decode_empty_string
    decoded = Base45.decode("")
    assert_equal("", decoded)
  end

  def test_encode_single_byte
    original = "a"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_two_bytes
    original = "ab"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_odd_length
    original = "abc"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_decode_invalid_length
    assert_raise(Base45::DecodeError) do
      Base45.decode("A")
    end
  end

  def test_decode_invalid_character
    assert_raise(Base45::DecodeError) do
      Base45.decode("@#")
    end
  end

  def test_encode_binary_data
    original = "\x01\x02\x03\x04\x05"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_long_string
    original = "The quick brown fox jumps over the lazy dog"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_alphabet_length
    assert_equal(45, Base45::ALPHABET.length)
  end

  def test_encode_all_bytes
    original = "\x00\xFF"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end

  def test_encode_qr_friendly_chars
    # Base45 alphabet should be QR code alphanumeric mode friendly
    original = "TEST123"
    encoded = Base45.encode(original)
    decoded = Base45.decode(encoded)
    assert_equal(original, decoded)
  end
end

class BaseEncodingComparisonTest < Picotest::Test
  def test_same_input_different_outputs
    original = "test"

    encoded58 = Base58.encode(original)
    encoded62 = Base62.encode(original)
    encoded45 = Base45.encode(original)

    # All should be different
    assert(encoded58 != encoded62)
    assert(encoded62 != encoded45)
    assert(encoded58 != encoded45)

    # All should decode to same original
    assert_equal(original, Base58.decode(encoded58))
    assert_equal(original, Base62.decode(encoded62))
    assert_equal(original, Base45.decode(encoded45))
  end

  def test_empty_string_all_encodings
    original = ""

    assert_equal("", Base58.encode(original))
    assert_equal("", Base62.encode(original))
    assert_equal("", Base45.encode(original))
  end

  def test_binary_data_all_encodings
    original = "\x00\x01\x02\x03\x04\x05\xFF"

    encoded58 = Base58.encode(original)
    encoded62 = Base62.encode(original)
    encoded45 = Base45.encode(original)

    assert_equal(original, Base58.decode(encoded58))
    assert_equal(original, Base62.decode(encoded62))
    assert_equal(original, Base45.decode(encoded45))
  end
end
