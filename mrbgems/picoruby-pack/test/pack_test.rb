class PackTest < Picotest::Test
  def test_pack_C
    assert_equal "A", [65].pack("C")
  end

  def test_pack_N
    assert_equal "\x00\x00\x00\x01", [1].pack("N")
  end

  def test_pack_n
    assert_equal "\x00\x01", [1].pack("n")
  end

  def test_unpack_C
    assert_equal [65], "A".unpack("C")
  end

  def test_unpack_N
    assert_equal [1], "\x00\x00\x00\x01".unpack("N")
  end

  def test_unpack_n
    assert_equal [1], "\x00\x01".unpack("n")
  end
end
