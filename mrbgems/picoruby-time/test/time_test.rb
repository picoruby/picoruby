class TimeTest < Picotest::Test
  def test_new_with_string_fields
    assert_equal(Time.new(2000, 1, 1).to_i, Time.new("2000", "1", "1").to_i)
    assert_equal(Time.new(2000, 1, 2, 3).to_i, Time.new("2000", 1, "2", "3").to_i)
  end

  def test_before_1970_with_a_fraction
    t = Time.at(-0.5)
    assert_equal(-1, t.to_i)
    assert_equal(500000, t.usec)
    t = Time.utc(1969, 12, 31, 23, 59, 59, 250000)
    assert_equal(-1, t.to_i)
    assert_equal(250000, t.usec)
    assert_equal(59, t.sec)
  end

  def test_round_floor_ceil_to_the_microsecond
    assert_equal(123000, Time.at(10, 123249).floor(3).usec)
    assert_equal(124000, Time.at(10, 123751).ceil(3).usec)
    assert_equal(123000, Time.at(10, 123249).round(3).usec)
    assert_equal(124000, Time.at(10, 123751).round(3).usec)
    assert_equal(999751, (Time.at(10) - 249 / 1000000.0).usec)
  end

  def test_usec_argument_range
    assert_raise(ArgumentError) { Time.utc(2000, 1, 1, 0, 0, 0, 1_000_000) }
    assert_raise(ArgumentError) { Time.utc(2000, 1, 1, 0, 0, 0, -1) }
    assert_raise(ArgumentError) { Time.local(2000, 1, 1, 0, 0, 0, 1_500_000) }
    assert_equal(999999, Time.utc(2000, 1, 1, 0, 0, 0, 999999).usec)
  end

  def test_seconds_fraction_stays_in_its_second
    t = Time.utc(2000, 1, 1, 0, 0, 0.9999997)
    assert_equal(946684800, t.to_i)
    assert_equal(0, t.sec)
    assert_equal(999999, t.usec)
  end

  # whatever zone the test runs in: the offset is the one in effect at
  # that time, daylight saving time included
  def test_utc_offset_is_that_of_the_time
    [1, 7].each do |mon|
      t = Time.local(2026, mon, 1, 12)
      fields_as_utc = Time.utc(t.year, t.mon, t.mday, t.hour, t.min, t.sec)
      assert_equal(fields_as_utc.to_i - t.to_i, t.utc_offset)
    end
  end

  def test_at_and_to_i
    assert_equal(0, Time.at(0).to_i)
    assert_equal(1234567890, Time.at(1234567890).to_i)
    assert_equal(-1, Time.at(-1).to_i)
  end

  def test_to_f
    assert_equal(Float, Time.at(0).to_f.class)
    assert_in_delta(1234567890.0, Time.at(1234567890).to_f)
  end

  def test_new_from_string
    t = Time.new("2026-11-05 12:00:00")
    assert_equal([2026, 11, 5, 12, 0, 0], [t.year, t.mon, t.mday, t.hour, t.min, t.sec])
    assert_false(t.utc?)
    t = Time.new("2026-11-05 12:00:00.5 +09:00")
    assert_equal(500000, t.usec)
    assert_equal(32400, t.utc_offset)
    assert_equal(1793847600, t.to_i)
    assert_true(Time.new("2026-11-05 12:00:00 UTC").utc?)
    assert_equal(1793880000, Time.new("2026-11-05 12:00:00 UTC").to_i)
    assert_true(Time.new("2026-11-05T12:00:00Z").utc?)
    assert_equal(1793880000, Time.new("2026-11-05T12:00:00+09:00").to_i + 9 * 3600)
    assert_equal(1793880000, Time.new("2026-11-05 12:00:00 +0900").to_i + 9 * 3600)
    assert_equal(2026, Time.new("2026").year)
    assert_equal([1, 1, 0], [Time.new("2026").mon, Time.new("2026").mday, Time.new("2026").hour])
    assert_equal(123456, Time.new("2026-11-05 12:00:00.123456789").usec)
    assert_raise(ArgumentError) { Time.new("2026-11-05") }
    assert_raise(ArgumentError) { Time.new("2026-11-05 12:00") }
    assert_raise(ArgumentError) { Time.new("2026-11-05 12:00:00 JST") }
    assert_raise(ArgumentError) { Time.new("2026-11-05 25:00:00") }
    assert_raise(ArgumentError) { Time.new(" 2026-11-05 12:00:00") }
    assert_raise(ArgumentError) { Time.new("2026-11-05 12:00:00 ") }
    assert_raise(ArgumentError) { Time.new("2026-1-5 1:2:3") }
    assert_raise(ArgumentError) { Time.new("2026-11-05 12:00:00", 1) }
  end

  def test_new_in_keyword
    assert_true(Time.new("2026-11-05 12:00:00", in: "UTC").utc?)
    assert_equal(1793880000, Time.new("2026-11-05 12:00:00", in: "UTC").to_i)
    assert_equal(32400, Time.new("2026-11-05 12:00:00", in: "+09:00").utc_offset)
    # the string's own zone wins
    assert_equal(3600, Time.new("2026-11-05 12:00:00 +01:00", in: "UTC").utc_offset)
    assert_true(Time.new(2026, 11, 5, 12, 0, 0, in: "UTC").utc?)
    assert_equal(1793880000, Time.new(2026, 11, 5, 12, 0, 0, in: "UTC").to_i)
    assert_equal(32400, Time.new(2026, 11, 5, 12, 0, 0, in: "+09:00").utc_offset)
    assert_equal(2026, Time.new(2026, in: "UTC").year)
    assert_raise(ArgumentError) { Time.new(2026, 11, 5, 12, 0, 0, "+09:00", in: "UTC") }
    assert_true(Time.new(in: "UTC").utc?)
    assert_equal(32400, Time.new(in: "+09:00").utc_offset)
  end

  def test_utc_and_local_take_usec
    assert_equal(123456, Time.utc(2026, 11, 5, 12, 0, 0, 123456).usec)
    assert_equal(1793880000, Time.utc(2026, 11, 5, 12, 0, 0, 123456).to_i)
    assert_equal(123456, Time.local(2026, 11, 5, 12, 0, 0, 123456).usec)
    assert_equal(500000, Time.utc(2026, 11, 5, 12, 0, 0, 500000.0).usec)
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
    # the 7th argument of Time.local is the microseconds
    assert_equal(0, Time.local(2025, 1, 1, 0, 0, 0, 0).usec)
    assert_raise(ArgumentError) { Time.local(2025, 1, 1, 0, 0, 0, 0, 0) }
    assert_raise(ArgumentError) { Time.new(2025, 1, 1, 0, 0, 0, 0, 0) }
    # the 7th argument of Time.new is the UTC offset
    assert_equal(0, Time.new(2025, 1, 1, 0, 0, 0, 0).utc_offset)
    assert_raise(ArgumentError) { Time.at }
    assert_raise(ArgumentError) { Time.at(1, 2, :usec, 4) }
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
    # CRuby prints the fraction without trailing zeros
    assert(t.inspect.include?("2025-03-04 05:06:07.5 "))
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

  # ---- UTC, offsets and zones ----

  def test_utc_constructor
    t = Time.utc(2025, 3, 4, 5, 6, 7)
    assert_true(t.utc?)
    assert_equal(0, t.utc_offset)
    assert_equal("UTC", t.zone)
    assert_equal(1741064767, t.to_i)
    assert_equal(5, t.hour)
    assert_equal(2, t.wday)
    assert_equal(63, t.yday)
    assert_equal("2025-03-04 05:06:07 UTC", t.to_s)
    assert_equal(Time.utc(2025, 3, 4, 5, 6, 7).to_i, Time.gm(2025, 3, 4, 5, 6, 7).to_i)
  end

  def test_utc_and_getutc
    local = Time.at(1741064767)
    assert_false(local.utc?)
    copy = local.getutc
    assert_true(copy.utc?)
    assert_false(local.utc?)
    assert_equal(5, copy.hour)
    assert_equal(local.to_i, copy.to_i)
    assert_equal(local, copy)
    local.utc
    assert_true(local.utc?)
    assert_equal(5, local.hour)
  end

  # A zone object gives the offset it has at the time, whichever way its
  # utc_to_local hands the local time back
  class WallClockZone
    def utc_to_local(t) = Time.utc(t.year, t.month, t.day, t.hour + 9, t.min, t.sec)
  end

  class OffsetZone
    def utc_to_local(t) = t.getlocal("-05:00")
  end

  def test_getlocal_and_localtime_with_a_zone_object
    t = Time.utc(2025, 3, 4, 5, 6, 7)
    l = t.getlocal(WallClockZone.new)
    assert_equal(9 * 3600, l.utc_offset)
    assert_equal(14, l.hour)
    assert_equal(t.to_i, l.to_i)
    assert_equal(-5 * 3600, t.getlocal(OffsetZone.new).utc_offset)
    t.localtime(WallClockZone.new)
    assert_equal(9 * 3600, t.utc_offset)
    assert_raise(ArgumentError) { t.getlocal(Object.new) }
  end

  def test_getlocal_and_localtime
    t = Time.utc(2025, 3, 4, 5, 6, 7)
    l = t.getlocal
    assert_false(l.utc?)
    assert_equal(t.to_i, l.to_i)
    assert_equal(t.hour, (l.hour - l.utc_offset / 3600) % 24)
    t.localtime
    assert_false(t.utc?)
  end

  def test_fixed_offset
    t = Time.new(2025, 3, 4, 5, 6, 7, "+09:00")
    assert_equal(32400, t.utc_offset)
    assert_equal(5, t.hour)
    assert_equal(1741032367, t.to_i)
    assert_nil(t.zone)
    assert_equal("2025-03-04 05:06:07 +0900", t.to_s)
    assert_equal(1741032367, Time.new(2025, 3, 4, 5, 6, 7, "+0900").to_i)
    assert_equal(1741032367 + 9 * 3600, Time.new(2025, 3, 4, 5, 6, 7, "UTC").to_i)
    assert_true(Time.new(2025, 3, 4, 5, 6, 7, "Z").utc?)
    assert_equal(1741032367, Time.new(2025, 3, 4, 5, 6, 7, 32400).to_i)
    assert_raise(ArgumentError) { Time.new(2025, 3, 4, 5, 6, 7, "nowhere") }
    assert_equal(15, t.getlocal("-05:00").hour)
  end

  def test_ten_argument_form
    # Time.local(sec, min, hour, mday, mon, year, wday, yday, isdst, zone), the order of to_a
    t = Time.local(7, 6, 5, 4, 3, 2025, nil, nil, false, nil)
    assert_equal([2025, 3, 4, 5, 6, 7], [t.year, t.mon, t.mday, t.hour, t.min, t.sec])
    u = Time.utc(7, 6, 5, 4, 3, 2025, nil, nil, false, nil)
    assert_true(u.utc?)
    assert_equal(1741064767, u.to_i)
    assert_raise(ArgumentError) { Time.local(1, 2, 3, 4, 5, 6, 7, 8, 9) }
  end

  def test_rational_seconds
    skip "no Rational in this build" unless Object.const_defined?(:Rational) && respond_to?(:Rational, true)
    t = Time.utc(2025, 3, 4, 5, 6, Rational(15, 2))
    assert_equal(7, t.sec)
    assert_equal(500000, t.usec)
  end

  def test_arithmetic_keeps_the_zone
    t = Time.utc(2025, 3, 4, 5, 6, 7)
    assert_true((t + 60).utc?)
    assert_true((t - 60).utc?)
    assert_equal(7, (t + 60).min)
    f = Time.new(2025, 3, 4, 5, 6, 7, "+09:00")
    assert_equal(32400, (f + 1).utc_offset)
  end

  def test_at_with_fraction
    assert_equal(500000, Time.at(1, 500000).usec)
    assert_equal(500000, Time.at(1, 500, :millisecond).usec)
    assert_equal(1, Time.at(1, 1500, :nsec).usec)
    assert_equal(250000, Time.at(1.25).usec)
    assert_equal(1, Time.at(1.25).to_i)
  end

  def test_eql_and_hash
    a = Time.at(1741064767)
    b = Time.at(1741064767)
    assert_true(a.eql?(b))
    assert_equal(a.hash, b.hash)
    assert_false(a.eql?(Time.at(1741064768)))
    assert_true(a.eql?(b.getutc))
  end

  def test_inspect_fraction
    assert_equal("2025-03-04 05:06:07 UTC", Time.utc(2025, 3, 4, 5, 6, 7).inspect)
    assert_equal("2025-03-04 05:06:07.5 UTC", Time.at(1741064767, 500000).utc.inspect)
  end

  def test_to_a
    a = Time.utc(2025, 3, 4, 5, 6, 7).to_a
    assert_equal([7, 6, 5, 4, 3, 2025, 2, 63, false, "UTC"], a)
  end

  def test_day_and_month_aliases
    t = Time.utc(2025, 3, 4)
    assert_equal(4, t.day)
    assert_equal(3, t.month)
  end

  # ---- strftime ----

  def test_strftime_basic
    t = Time.utc(2025, 3, 4, 5, 6, 7)
    assert_equal("2025-03-04 05:06:07", t.strftime("%Y-%m-%d %H:%M:%S"))
    assert_equal("2025-03-04T05:06:07", t.strftime("%FT%T"))
    assert_equal("Tue Mar  4 05:06:07 2025", t.strftime("%c"))
    assert_equal("Tuesday March 063", t.strftime("%A %B %j"))
    assert_equal("Tue Mar", t.strftime("%a %b"))
    assert_equal("05 AM  5 am", t.strftime("%I %p %l %P"))
    assert_equal("4 04  4", t.strftime("%-d %d %e"))
    assert_equal("%", t.strftime("%%"))
    assert_equal("1741064767", t.strftime("%s"))
    assert_equal("TUE TUE am", t.strftime("%^a %#a %#p"))
    assert_equal("0000002025", t.strftime("%10Y"))
    assert_equal("25 20", t.strftime("%y %C"))
    assert_equal("2 2 09 09 10", t.strftime("%u %w %U %W %V"))
  end

  def test_strftime_zone
    assert_equal("+0000 +00:00 +00:00:00 UTC", Time.utc(2025).strftime("%z %:z %::z %Z"))
    assert_equal("+0900 +09:00", Time.new(2025, 1, 1, 0, 0, 0, "+09:00").strftime("%z %:z"))
    assert_equal("-0500", Time.new(2025, 1, 1, 0, 0, 0, "-05:00").strftime("%z"))
  end

  def test_strftime_fraction
    t = Time.at(1741064767, 123456).utc
    assert_equal("123 123456 123456000 12", t.strftime("%L %6N %N %2N"))
  end

  def test_strftime_week_based_year
    assert_equal("2020-53", Time.utc(2021, 1, 3).strftime("%G-%V"))
    assert_equal("2021-01", Time.utc(2021, 1, 4).strftime("%G-%V"))
  end

  # a Date-like receiver: the fields Strftime reads, nothing else. The
  # include happens in the test: the runner loads this file on CRuby too,
  # where Time::Strftime does not exist.
  class StrftimeDay
    def year = 2025
    def mon = 3
    def mday = 4
    def hour = 0
    def min = 0
    def sec = 0
    def wday = 2
    def yday = 63
    def nsec = 0
    def utc_offset = 0
    def zone = "+00:00"
    def to_i = 1741046400
  end

  def test_strftime_is_a_mixin
    assert_true(Time.ancestors.include?(Time::Strftime))
    StrftimeDay.include(Time::Strftime)
    day = StrftimeDay.new
    assert_equal("2025-03-04 Tue 00:00:00 +00:00", day.strftime("%F %a %T %:z"))
    assert_equal("March 4 1741046400 2025-10", day.strftime("%B %-d %s %G-%V"))
  end

  def test_iso8601
    assert_equal("2025-03-04T05:06:07Z", Time.utc(2025, 3, 4, 5, 6, 7).iso8601)
    assert_equal("2025-03-04T05:06:07.500Z", Time.at(1741064767, 500000).utc.iso8601(3))
    assert_equal("2025-03-04T05:06:07+09:00", Time.new(2025, 3, 4, 5, 6, 7, "+09:00").xmlschema)
  end

  def test_round_floor_ceil
    t = Time.at(10, 500000)
    assert_equal(11, t.round.to_i)
    assert_equal(10, t.floor.to_i)
    assert_equal(11, t.ceil.to_i)
    assert_equal(0, t.round.usec)
    assert_equal(123000, Time.at(10, 123456).round(3).usec)
    assert_equal(123400, Time.at(10, 123456).floor(4).usec)
    assert_equal(123460, Time.at(10, 123456).ceil(5).usec)
  end

  def test_comparable
    t = Time.at(100)
    assert_true(t.between?(Time.at(99), Time.at(101)))
    assert_equal(Time.at(101), t.clamp(Time.at(101), Time.at(102)))
    assert_equal(Time.at(101), t.succ)
  end

  def test_subsec
    assert_equal(0, Time.at(1).subsec)
    assert_equal(250000000, Time.at(1, 250000).nsec)
  end
end
