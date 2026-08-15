class TimeTest < Picotest::Test
  def test_at_and_to_i
    assert_equal(0, Time.at(0).to_i)
    assert_equal(1234567890, Time.at(1234567890).to_i)
    assert_equal(-1, Time.at(-1).to_i)
  end

  def test_to_f
    assert_equal(Float, Time.at(0).to_f.class)
    assert_in_delta(1234567890.0, Time.at(1234567890).to_f)
  end

  def test_local_fields
    t = Time.local(2025, 3, 4, 5, 6, 7)
    assert_equal(2025, t.year)
    assert_equal(3, t.mon)
    assert_equal(4, t.mday)
    assert_equal(5, t.hour)
    assert_equal(6, t.min)
    assert_equal(7, t.sec)
    assert_equal(0, t.usec)
    # 2025-03-04 is a Tuesday
    assert_equal(2, t.wday)
  end

  def test_local_defaults
    t = Time.local(2025)
    assert_equal(2025, t.year)
    assert_equal(1, t.mon)
    assert_equal(1, t.mday)
    assert_equal(0, t.hour)
    assert_equal(0, t.min)
    assert_equal(0, t.sec)
    assert_equal(0, t.usec)
  end

  def test_new_with_args
    t = Time.new(2025, 3, 4, 5, 6, 7)
    assert_equal(2025, t.year)
    assert_equal(3, t.mon)
    assert_equal(4, t.mday)
    assert_equal(5, t.hour)
    assert_equal(6, t.min)
    assert_equal(7, t.sec)
  end

  def test_new_without_args_returns_current_time
    d = Time.new.to_i - Time.now.to_i
    assert(d.abs <= 1)
  end

  def test_now_is_reasonably_recent
    # 1700000000 is 2023-11-14; any correct clock is after this
    assert(1700000000 < Time.now.to_i)
  end

  def test_float_sec
    t = Time.local(2025, 1, 1, 0, 0, 1.5)
    assert_equal(1, t.sec)
    assert_equal(500000, t.usec)
  end

  def test_float_sec_rounds_usec
    # 0.1 is not exactly representable; usec must round to 100000,
    # not truncate to 99999
    t = Time.local(2025, 1, 1, 0, 0, 0.1)
    assert_equal(100000, t.usec)
  end

  def test_float_sec_to_f
    t0 = Time.local(2025, 1, 1, 0, 0, 0)
    t1 = Time.local(2025, 1, 1, 0, 0, 1.5)
    assert_in_delta(1.5, t1.to_f - t0.to_f)
  end

  def test_sec_out_of_range
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, 61) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, -1) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, 60.5) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, -0.5) }
  end

  def test_sec_infinity
    inf = 1.0 / 0.0
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, inf) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, -inf) }
  end

  def test_sec_nan
    nan = 0.0 / 0.0
    if femtoruby?
      assert_raise(RangeError) { Time.local(2025, 1, 1, 0, 0, nan) }
    else
      assert_raise(FloatDomainError) { Time.local(2025, 1, 1, 0, 0, nan) }
    end
  end

  def test_sec_60_is_allowed
    # Leap second notation is accepted
    t = Time.local(2025, 12, 31, 23, 59, 60)
    assert_equal(Time, t.class)
  end

  def test_field_out_of_range
    assert_raise(ArgumentError) { Time.local(2025, 0) }
    assert_raise(ArgumentError) { Time.local(2025, 13) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 0) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 32) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 24) }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 60) }
  end

  def test_wrong_number_of_arguments
    assert_raise(ArgumentError) { Time.local }
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, 0, 0) }
    assert_raise(ArgumentError) { Time.new(2025, 1, 1, 0, 0, 0, 0) }
    assert_raise(ArgumentError) { Time.at }
    assert_raise(ArgumentError) { Time.at(1, 2) }
  end

  def test_comparison
    t1 = Time.at(100)
    t2 = Time.at(200)
    t3 = Time.at(100)
    assert(t1 < t2)
    assert(t1 <= t2)
    assert(t1 <= t3)
    assert(t2 > t1)
    assert(t2 >= t1)
    assert(t1 >= t3)
    assert(t1 == t3)
    assert_false(t1 == t2)
    assert_equal(-1, t1 <=> t2)
    assert_equal(0, t1 <=> t3)
    assert_equal(1, t2 <=> t1)
  end

  def test_comparison_with_other_type
    assert_raise(ArgumentError) { Time.at(0) < 1 }
    assert_raise(ArgumentError) { Time.at(0) > 1 }
  end

  def test_add
    t = Time.at(100) + 50
    assert_equal(Time, t.class)
    assert_equal(150, t.to_i)
    t = Time.at(100) + 1.5
    assert_in_delta(101.5, t.to_f)
    assert_raise(ArgumentError) { Time.at(0) + "1" }
  end

  def test_sub_scalar
    t = Time.at(100) - 30
    assert_equal(Time, t.class)
    assert_equal(70, t.to_i)
    t = Time.at(100) - 0.5
    assert_in_delta(99.5, t.to_f)
  end

  def test_sub_time
    d = Time.at(200) - Time.at(100)
    assert_equal(Float, d.class)
    assert_in_delta(100.0, d)
    d = Time.at(100) - Time.at(200)
    assert_in_delta(-100.0, d)
  end

  def test_roundtrip_local_at
    t = Time.local(2025, 3, 4, 5, 6, 7)
    u = Time.at(t.to_i)
    assert(t == u)
    assert_equal(t.to_s, u.to_s)
  end

  def test_to_s
    t = Time.local(2025, 3, 4, 5, 6, 7)
    assert(t.to_s.include?("2025-03-04 05:06:07"))
  end

  def test_inspect
    t = Time.local(2025, 3, 4, 5, 6, 7.5)
    assert(t.inspect.include?("2025-03-04 05:06:07.500000"))
  end

  def test_wday_predicates
    # 2025-03-04 is a Tuesday, 2025-03-09 is a Sunday
    t = Time.local(2025, 3, 4)
    assert(t.tuesday?)
    assert_false(t.monday?)
    assert_false(t.sunday?)
    assert(Time.local(2025, 3, 9).sunday?)
  end

  def test_unixtime_offset
    assert_equal(Integer, Time.unixtime_offset.class)
  end

  def test_time_methods
    # sig/time.rbs declares self.time_methods, but the mruby VM impl
    # registers it as an instance method only, so call it on an instance
    assert_equal(Time::TimeMethods, Time.time_methods.class)
  end
end
