class PackTest < Picotest::Test
  def test_array_pack_c_unsigned
    result = [65, 66, 67].pack('CCC')
    assert_equal 'ABC', result
  end

  def test_array_pack_c_single
    result = [255].pack('C')
    assert_equal "\xFF", result
  end

  def test_array_pack_c_signed
    result = [-1, 0, 1].pack('ccc')
    assert_equal "\xFF\x00\x01", result
  end

  def test_array_pack_n_big_endian
    result = [0x12345678].pack('N')
    assert_equal "\x12\x34\x56\x78", result
  end

  def test_array_pack_v_little_endian
    result = [0x12345678].pack('V')
    assert_equal "\x78\x56\x34\x12", result
  end

  def test_array_pack_n_16bit
    result = [0x1234].pack('n')
    assert_equal "\x12\x34", result
  end

  def test_array_pack_v_16bit
    result = [0x1234].pack('v')
    assert_equal "\x34\x12", result
  end

  def test_array_pack_s_native
    result = [0x1234].pack('S')
    assert_equal 2, result.size
  end

  def test_array_pack_l_native
    result = [0x12345678].pack('L')
    assert_equal 4, result.size
  end

  def test_array_pack_multiple
    result = [1, 2, 3, 4].pack('CCCC')
    assert_equal "\x01\x02\x03\x04", result
  end

  def test_array_pack_mixed_types
    result = [0x12, 0x3456, 0x789ABCDE].pack('CnN')
    assert_equal "\x12\x34\x56\x78\x9A\xBC\xDE", result
  end

  def test_string_unpack_c_unsigned
    result = 'ABC'.unpack('CCC')
    assert_equal [65, 66, 67], result
  end

  def test_string_unpack_c_single
    result = "\xFF".unpack('C')
    assert_equal [255], result
  end

  def test_string_unpack_c_signed
    result = "\xFF\x00\x01".unpack('ccc')
    assert_equal [-1, 0, 1], result
  end

  def test_string_unpack_n_big_endian
    result = "\x12\x34\x56\x78".unpack('N')
    assert_equal [0x12345678], result
  end

  def test_string_unpack_v_little_endian
    result = "\x78\x56\x34\x12".unpack('V')
    assert_equal [0x12345678], result
  end

  def test_string_unpack_n_16bit
    result = "\x12\x34".unpack('n')
    assert_equal [0x1234], result
  end

  def test_string_unpack_v_16bit
    result = "\x34\x12".unpack('v')
    assert_equal [0x1234], result
  end

  def test_string_unpack_multiple
    result = "\x01\x02\x03\x04".unpack('CCCC')
    assert_equal [1, 2, 3, 4], result
  end

  def test_string_unpack_mixed_types
    result = "\x12\x34\x56\x78\x9A\xBC\xDE".unpack('CnN')
    assert_equal [0x12, 0x3456, 0x789ABCDE], result
  end

  def test_round_trip
    original = [1, 2, 3, 4, 5]
    packed = original.pack('CCCCC')
    unpacked = packed.unpack('CCCCC')
    assert_equal original, unpacked
  end

  def test_array_pack_c_star
    result = [1, 2, 3, 4, 5].pack('C*')
    assert_equal "\x01\x02\x03\x04\x05", result
  end

  def test_string_unpack_c_star
    result = "\x01\x02\x03\x04\x05".unpack('C*')
    assert_equal [1, 2, 3, 4, 5], result
  end

  def test_array_pack_invalid_format
    assert_raise(ArgumentError) do
      [1].pack('X')
    end
  end

  def test_string_unpack_invalid_format
    assert_raise(ArgumentError) do
      "\x01".unpack('X')
    end
  end
end
