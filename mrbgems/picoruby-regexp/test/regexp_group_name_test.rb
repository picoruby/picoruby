class RegexpGroupNameTest < Picotest::Test
  def test_name_may_hold_any_byte_after_the_first
    assert_equal "x", /(?<a-b>x)/.match("x")[:"a-b"]
    assert_equal "x", /(?<a.b>x)y/.match("xy")["a.b"]
    assert_equal "1.2", "1.2"[/(?<dec-octet>\d)\.\g<dec-octet>/]
  end

  def test_name_may_not_start_with_a_digit_or_a_minus
    assert_raise(RegexpError) { Regexp.new("(?<1a>x)") }
    assert_raise(RegexpError) { Regexp.new("(?<-a>x)") }
    assert_equal "x", /(?<+a>x)/.match("x")["+a"]
  end

  # beside a named group the plain ones take no group slot: 15 named
  # groups fit beside any number of plain ones
  def test_plain_groups_beside_named_ones_take_no_slot
    src = ""
    i = 0
    while i < 15
      src << "(?<n#{i}>a)"
      i += 1
    end
    src << "()" * 60
    md = Regexp.new(src).match("a" * 15)
    assert_equal 16, md.size
    assert_equal "a", md[:n14]
  end
end
