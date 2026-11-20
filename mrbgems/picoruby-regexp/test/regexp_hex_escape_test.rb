class RegexpHexEscapeTest < Picotest::Test
  def setup
    require 'regexp'
  end

  def test_hex_escape_ascii
    assert_equal 0, Regexp.new("\\x41") =~ "A"
    assert_equal 1, Regexp.new("[\\x30-\\x39]") =~ "a5"
  end

  def test_hex_escapes_form_a_utf8_sequence
    assert_equal 0, Regexp.new("\\A\\xEF\\xBB\\xBF") =~ "\xEF\xBB\xBFrun"
    assert_equal 1, Regexp.new("\\xe3\\x81\\x82") =~ "xあ"
    assert_equal 0, Regexp.new("\\xC3\\xA9+") =~ "éé"
    assert_equal "é", "café".match(Regexp.new("[\\xC3\\xA9]"))[0]
    assert_equal 0, Regexp.new("\\xF0\\x9F\\x98\\x80") =~ "😀"
  end

  def test_invalid_multibyte_escape
    assert_raise(RegexpError) { Regexp.new("\\xFF") }
    assert_raise(RegexpError) { Regexp.new("\\xE3\\x81") }
    assert_raise(RegexpError) { Regexp.new("\\xE3a") }
    assert_raise(RegexpError) { Regexp.new("\\xC0\\x80") }
    assert_raise(RegexpError) { Regexp.new("\\xED\\xA0\\x80") }
  end
end
