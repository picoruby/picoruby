class RegexpAnchorTest < Picotest::Test
  def setup
    require 'regexp'
  end

  # ^ and $ hold at line boundaries; \A and \z only at the ends
  def test_line_anchors
    assert_equal 2, "a\nb" =~ /^b/
    assert_equal 0, "a\nb" =~ /a$/
    assert_equal 1, "ab\n" =~ /b$/
    assert_nil "a\nb" =~ /\Ab/
    assert_nil "ab\n" =~ /b\z/
    assert_equal ["foo", "bar"], "foo\nbar".scan(/^\w+$/)
  end

  # ^ does not hold after a newline that ends the string
  def test_line_begin_at_the_end
    assert_nil "x\n" =~ /^$/
    assert_equal 0, "\n\n" =~ /^$/
  end

  # \Z holds at the end or before a newline that ends the string
  def test_end_before_final_newline
    assert_equal 1, "ab\n" =~ /b\Z/
    assert_equal 2, "a\nb\n" =~ /b\Z/
    assert_nil "ab\nc" =~ /b\Z/
  end

  # \G holds where the search began: the start, the pos given, or the
  # end of the match before in scan and gsub
  def test_search_start
    assert_equal ["a", "a"], "aab".scan(/\Ga/)
    assert_equal "xx", "abab".gsub(/\Gab/, "x")
    assert_equal "xbab", "abab".gsub(/\Ga/, "x")
    assert_equal 1, "xab".match(/\Gab/, 1)&.begin(0)
    assert_nil "xab" =~ /\Gab/
  end
end
