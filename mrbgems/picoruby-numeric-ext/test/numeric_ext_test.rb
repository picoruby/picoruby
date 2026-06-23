class NumericExtTest < Picotest::Test
  # Float#round
  def test_round_positive
    assert_equal 4, 3.5.round
    assert_equal 3, 3.4.round
  end

  def test_round_negative
    assert_equal(-4, (-3.5).round)
    assert_equal(-3, (-3.4).round)
  end

  def test_round_zero
    assert_equal 0, 0.0.round
  end

  def test_round_with_ndigits
    assert_equal 3.14, 3.14159.round(2)
    assert_equal 3.142, 3.14159.round(3)
  end

  def test_round_with_negative_ndigits
    assert_equal 130, 125.4.round(-1)
  end

  def test_round_returns_integer_when_ndigits_zero
    assert_equal Integer, 3.5.round.class
  end

  def test_round_returns_float_when_ndigits_positive
    assert_equal Float, 3.5.round(1).class
  end

  # Float#floor
  def test_floor_positive
    assert_equal 3, 3.7.floor
  end

  def test_floor_negative
    assert_equal(-4, (-3.7).floor)
  end

  def test_floor_with_ndigits
    assert_equal 3.14, 3.14159.floor(2)
  end

  def test_floor_with_negative_ndigits
    assert_equal 120, 123.4.floor(-1)
  end

  def test_floor_returns_integer_when_ndigits_zero
    assert_equal Integer, 3.7.floor.class
  end

  def test_floor_returns_float_when_ndigits_positive
    assert_equal Float, 3.7.floor(1).class
  end

  # Float#ceil
  def test_ceil_positive
    assert_equal 4, 3.2.ceil
  end

  def test_ceil_negative
    assert_equal(-3, (-3.2).ceil)
  end

  def test_ceil_with_ndigits
    assert_equal 3.15, 3.14159.ceil(2)
  end

  def test_ceil_with_negative_ndigits
    assert_equal 200, 123.4.ceil(-2)
  end

  def test_ceil_returns_integer_when_ndigits_zero
    assert_equal Integer, 3.2.ceil.class
  end

  def test_ceil_returns_float_when_ndigits_positive
    assert_equal Float, 3.2.ceil(1).class
  end

  # Float#%
  def test_mod_positive
    assert_equal 1.0, 7.0 % 3.0
  end

  def test_mod_negative_left
    assert_equal 2.0, (-7.0) % 3.0
  end

  def test_mod_negative_right
    assert_equal(-2.0, 7.0 % (-3.0))
  end

  def test_mod_both_negative
    assert_equal(-1.0, (-7.0) % (-3.0))
  end

  def test_mod_with_integer
    assert_equal 1.0, 7.0 % 3
  end

  def test_mod_returns_float
    assert_equal Float, (7.0 % 3.0).class
  end

  def test_mod_zero_division
    assert_raise(ZeroDivisionError) { 1.0 % 0 }
  end

  def test_mod_zero_division_float
    assert_raise(ZeroDivisionError) { 1.0 % 0.0 }
  end

  # Sign predicates (Integer)
  def test_integer_zero?
    assert_equal true, 0.zero?
    assert_equal false, 1.zero?
  end

  def test_integer_nonzero?
    assert_equal nil, 0.nonzero?
    assert_equal 7, 7.nonzero?
  end

  def test_integer_positive?
    assert_equal true, 1.positive?
    assert_equal false, 0.positive?
    assert_equal false, (-1).positive?
  end

  def test_integer_negative?
    assert_equal true, (-1).negative?
    assert_equal false, 0.negative?
    assert_equal false, 1.negative?
  end

  # Sign predicates (Float)
  def test_float_zero?
    assert_equal true, 0.0.zero?
    assert_equal false, 0.1.zero?
  end

  def test_float_nonzero?
    assert_equal nil, 0.0.nonzero?
    assert_equal 2.5, 2.5.nonzero?
  end

  def test_float_positive?
    assert_equal true, 0.1.positive?
    assert_equal false, 0.0.positive?
    assert_equal false, (-0.1).positive?
  end

  def test_float_negative?
    assert_equal true, (-0.1).negative?
    assert_equal false, 0.0.negative?
    assert_equal false, 0.1.negative?
  end
end
