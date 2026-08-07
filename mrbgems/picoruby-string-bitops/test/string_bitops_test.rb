# Tests for picoruby-string-bitops (FemtoRuby / mruby/c)
# Ported from CRuby [Feature #22118] test suite.

class StringBitopsTest < Picotest::Test
  def test_bit_get
    s = "\xAA\x80"
    assert_equal 0, s.bit_get(0)
    assert_equal 1, s.bit_get(1)
    assert_equal 1, s.bit_get(7)
    assert_equal 1, s.bit_get(0, lsb_first: false)
    assert_equal 0, s.bit_get(1, lsb_first: false)
    assert_equal 1, s.bit_get(8, lsb_first: false)
    assert_nil s.bit_get(16)
    assert_raise(IndexError) { s.bit_get(-1) }
    assert_raise(ArgumentError) { s.bit_get(0, lsb_first: nil) }
    assert_raise(TypeError) { s.bit_get("1") }
  end

  def test_bit_set_p
    s = "\xAA\x80"
    assert_equal false, s.bit_set?(0)
    assert_equal true, s.bit_set?(1)
    assert_equal true, s.bit_set?(7)
    assert_equal true, s.bit_set?(0, lsb_first: false)
    assert_equal false, s.bit_set?(1, lsb_first: false)
    assert_equal true, s.bit_set?(8, lsb_first: false)
    assert_nil s.bit_set?(16)
    assert_raise(IndexError) { s.bit_set?(-1) }
    assert_raise(ArgumentError) { s.bit_set?(0, lsb_first: nil) }
  end

  def test_bit_set_clear_flip
    s = "\x00"
    assert_equal s.object_id, s.bit_set(1).object_id
    assert_equal "\x02", s
    assert_equal s.object_id, s.bit_clear(1).object_id
    assert_equal "\x00", s
    assert_equal s.object_id, s.bit_flip(1).object_id
    assert_equal "\x02", s
    s.bit_flip(1)
    assert_equal "\x00", s

    s.bit_set(1, lsb_first: false)
    assert_equal "\x40", s
    s.bit_clear(1, lsb_first: false)
    assert_equal "\x00", s

    s = "\x00\x00"
    s.bit_set(8, lsb_first: false)
    assert_equal "\x00\x80", s
    s.bit_clear(8, lsb_first: false)
    assert_equal "\x00\x00", s
    s.bit_flip(8, lsb_first: false)
    assert_equal "\x00\x80", s

    assert_raise(IndexError) { "\x00".bit_set(8) }
    assert_raise(IndexError) { "\x00".bit_set(-1) }
    assert_raise(IndexError) { "\x00".bit_clear(8) }
    assert_raise(IndexError) { "\x00".bit_clear(-1) }
    assert_raise(IndexError) { "\x00".bit_flip(8) }
    assert_raise(IndexError) { "\x00".bit_flip(-1) }
    assert_raise(ArgumentError) { "\x00".bit_set(0, lsb_first: nil) }
  end

  def test_bit_count
    assert_equal 0, "".bit_count
    assert_equal 0, "\x00".bit_count
    assert_equal 8, "\xFF".bit_count
    assert_equal 8, "\xAA\xF0".bit_count
    assert_raise(ArgumentError) { "\x00".bit_count(0) }
    assert_raise(ArgumentError) { "\x00".bit_count(lsb_first: false) }
  end

  def test_bit_count_long_strings
    # Exercise the word-at-a-time and unrolled paths.
    assert_equal 0, ("\x00" * 100).bit_count
    assert_equal 800, ("\xFF" * 100).bit_count
    assert_equal 400, ("\xAA" * 100).bit_count
    assert_equal 4 * 33, ("\x0F" * 33).bit_count
    # Short strings exercise the unaligned and tail paths.
    assert_equal 8 * 24, ("\xFF" * 24).bit_count
    assert_equal 4 * 20, ("\x0F" * 20).bit_count
  end

  def test_bitwise_not
    s = "\x00\xAA"
    result = s.bitwise_not
    assert_equal "\xFF\x55", result
    assert_not_equal s.object_id, result.object_id
    assert_equal "\x00\xAA", s

    assert_equal s.object_id, s.bitwise_not!.object_id
    assert_equal "\xFF\x55", s
  end

  def test_bitwise_and_or_xor
    assert_equal "\xC0", "\xF0".bitwise_and("\xCC")
    assert_equal "\xFC", "\xF0".bitwise_or("\x0C")
    assert_equal "\x3C", "\xF0".bitwise_xor("\xCC")

    s = "\xF0"
    assert_equal s.object_id, s.bitwise_and!("\xCC").object_id
    assert_equal "\xC0", s
    assert_equal s.object_id, s.bitwise_or!("\x0C").object_id
    assert_equal "\xCC", s
    assert_equal s.object_id, s.bitwise_xor!("\xFF").object_id
    assert_equal "\x33", s

    # Length mismatch: other longer, shorter, and empty.
    assert_raise(ArgumentError) { "\xF0".bitwise_and("\x00\x00") }
    assert_raise(ArgumentError) { "\xF0".bitwise_and("") }
    assert_raise(ArgumentError) { "\xF0".bitwise_or("\x00\x00") }
    assert_raise(ArgumentError) { "\xF0".bitwise_or("") }
    assert_raise(ArgumentError) { "\xF0".bitwise_xor("\x00\x00") }
    assert_raise(ArgumentError) { "\xF0".bitwise_xor("") }
    assert_raise(ArgumentError) { "\xF0".bitwise_and!("") }
    assert_raise(ArgumentError) { "\xF0".bitwise_or!("") }
    assert_raise(ArgumentError) { "\xF0".bitwise_xor!("") }
    assert_raise(ArgumentError) { "\x00\x00".bitwise_or!("\x00") }
    assert_raise(TypeError) { "\x00".bitwise_or(nil) }
    assert_raise(TypeError) { "\x00".bitwise_or(0) }
  end

  def test_bitwise_long_strings
    a = "\xAA" * 41
    b = "\x0F" * 41
    assert_equal "\x0A" * 41, a.bitwise_and(b)
    assert_equal "\xAF" * 41, a.bitwise_or(b)
    assert_equal "\xA5" * 41, a.bitwise_xor(b)
    assert_equal "\x55" * 41, a.bitwise_not
    assert_equal "\x00" * 41, a.bitwise_xor(a)

    c = a.dup
    c.bitwise_xor!(b)
    c.bitwise_xor!(b)
    assert_equal a, c

    # 20-byte strings exercise short-buffer word loops and the tail.
    e = "\xAA" * 20
    f = "\x0F" * 20
    assert_equal "\x0A" * 20, e.bitwise_and(f)
    assert_equal "\xAF" * 20, e.bitwise_or(f)
    assert_equal "\xA5" * 20, e.bitwise_xor(f)
    assert_equal "\x55" * 20, e.bitwise_not
    e.bitwise_xor!(f)
    assert_equal "\xA5" * 20, e
  end
end
