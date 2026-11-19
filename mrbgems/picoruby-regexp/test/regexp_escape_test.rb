class RegexpEscapeTest < Picotest::Test
  # a surrogate is no character
  def test_surrogate_escape
    assert_raise(RegexpError) { Regexp.new("\\uD800") }
    assert_raise(RegexpError) { Regexp.new("[\\u{DFFF}]") }
  end
end
