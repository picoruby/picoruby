# The values are computed, not written: a literal past mrb_int would not
# compile on an MRB_INT32 build. 1 << 30 is the last 31-bit fixnum
# of a word-boxing build; 1 << 31 is past mrb_int on MRB_INT32, where
# the irep makes a Bignum of it.
class AOTProbeTest < Picotest::Test
  def test_add_past_the_fixnum
    assert_equal(1 << 31, AOTProbe.add(1 << 30, 1 << 30))
  end

  def test_add_past_mrb_int
    assert_equal(1 << 32, AOTProbe.add(1 << 31, 1 << 31))
  end

  def test_add_negative_past_mrb_int
    assert_equal(-(1 << 32), AOTProbe.add(-(1 << 31), -(1 << 31)))
  end

  def test_mul_past_mrb_int
    assert_equal(10_000_000_000, AOTProbe.mul(100_000, 100_000))
  end

  def test_sub_of_two_bignums_is_small
    assert_equal(7, AOTProbe.sub((1 << 40) + 7, 1 << 40))
  end

  def test_div_and_mod_of_a_bignum
    assert_equal(1 << 20, AOTProbe.div(1 << 40, 1 << 20))
    assert_equal(5, AOTProbe.mod((1 << 40) + 5, 1 << 20))
  end

  def test_shift_into_a_bignum
    assert_equal(1 << 40, AOTProbe.shl(1, 40))
  end

  def test_neg_of_a_bignum
    assert_equal(-(1 << 40), AOTProbe.neg(1 << 40))
  end

  def test_a_bignum_passes_through
    assert_equal(1 << 40, AOTProbe.same(1 << 40))
  end

  def test_float_to_i_past_mrb_int
    assert_equal(4_000_000_000_000, AOTProbe.to_int(4.0e12))
  end

  def test_bignum_to_f
    assert_in_delta(1.099511627776e12, AOTProbe.to_float(1 << 40), 1.0)
  end

  def test_compare_past_mrb_int
    assert(AOTProbe.later?(1 << 31, (1 << 31) + 1))
    assert_false(AOTProbe.later?((1 << 31) + 1, 1 << 31))
  end

  def test_elapsed_microseconds_past_36_minutes
    now = (1 << 31) + 500
    assert_equal(1500, AOTProbe.elapsed(now, now - 1500))
    assert_equal(0, AOTProbe.elapsed(now - 1, now))
  end

  def test_clock_keeps_a_bignum_timestamp
    clock = AOTProbe::Clock.new
    clock.observe((1 << 31) - 100)
    assert_equal(200, clock.observe((1 << 31) + 100))
    assert_equal((1 << 31) + 100, clock.last_us)
  end
end
