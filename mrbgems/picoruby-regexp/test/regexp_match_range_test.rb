class RegexpMatchRangeTest < Picotest::Test
  def setup
    require 'regexp'
  end

  def test_match_data_aref_with_a_range
    m = /(a)(b)?(c)/.match("xacx")
    assert_equal ["a", nil, "c"], m[1..-1]
    assert_equal ["a"], m[1...2]
    assert_nil m[5..]
  end

  def test_match_data_aref_with_start_and_length
    m = /(a)(b)?(c)/.match("xacx")
    assert_equal ["ac", "a"], m[0, 2]
  end

  def test_match_data_aref_with_a_group
    m = /(a)(b)?(c)/.match("xacx")
    assert_equal "c", m[-1]
    assert_equal "a", m[1]
    assert_equal "a", /(?<n>a)/.match("a")["n"]
  end
end
