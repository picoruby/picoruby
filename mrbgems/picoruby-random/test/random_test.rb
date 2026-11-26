class RandomTest < Picotest::Test
  def setup
    require "random"
  end

  def test_urandom_returns_a_string_of_the_size
    assert_equal 16, Random.urandom(16).bytesize
    assert_equal 1000, Random.urandom(1000).bytesize
    assert_equal "", Random.urandom(0)
  end

  def test_urandom_differs_between_calls
    assert_not_equal Random.urandom(16), Random.urandom(16)
  end

  def test_urandom_rejects_a_negative_size
    assert_raise(ArgumentError) { Random.urandom(-1) }
  end

  def test_bytes_returns_a_string_of_the_size
    assert_equal 8, Random.bytes(8).bytesize
  end

  def test_rand_without_argument_is_a_float_below_one
    f = rand
    assert_equal Float, f.class
    assert(0.0 <= f && f < 1.0)
  end

  def test_rand_zero_is_a_float
    assert_equal Float, rand(0).class
  end

  def test_rand_integer_is_in_range
    i = 0
    while i < 100
      r = rand(10)
      assert(0 <= r && r < 10)
      i += 1
    end
  end

  def test_rand_integer_reaches_every_value
    seen = [false, false, false, false]
    i = 0
    while i < 200
      seen[rand(4)] = true
      i += 1
    end
    assert_equal [true, true, true, true], seen
  end

  def test_kernel_rand_takes_the_magnitude_of_a_negative_limit
    r = rand(-10)
    assert(0 <= r && r < 10)
  end

  def test_random_rand_rejects_a_negative_limit
    assert_raise(ArgumentError) { Random.rand(-1) }
  end

  def test_rand_float_limit_is_truncated
    assert_equal 0, rand(1.5)
  end

  def test_rand_integer_range
    r = rand(3..5)
    assert(3 <= r && r <= 5)
    assert_equal 3, rand(3...4)
  end

  def test_rand_empty_range_is_nil
    assert_nil rand(5..3)
    assert_nil rand(3...3)
  end

  def test_rand_float_range
    f = rand(1.0..2.0)
    assert_equal Float, f.class
    assert(1.0 <= f && f <= 2.0)
  end

  def test_random_rand_integer
    r = Random.rand(10)
    assert(0 <= r && r < 10)
  end
end
