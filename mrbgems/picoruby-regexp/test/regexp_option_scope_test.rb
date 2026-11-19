class RegexpOptionScopeTest < Picotest::Test
  # as in Onigmo, a(?i)b|c is a(?i:b|c)
  def test_inline_option_takes_the_alternatives_after_it
    re = /a(?i)b|c/
    assert_nil re =~ "C"
    assert_nil re =~ "c"
    assert_equal "ac", re.match("ac")[0]
    assert_equal "aC", re.match("aC")[0]
    assert_equal 0, /(?i)a(?-i)b|c/ =~ "Ab"
    assert_nil /(?i)a(?-i)b|c/ =~ "C"
  end
end
