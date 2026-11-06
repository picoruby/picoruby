class RegexpRepeatTest < Picotest::Test
  def test_possessive_repeat_gives_nothing_back
    assert_nil(/\Aa*+a\z/ =~ "aa")
    assert_nil(/\Aa++a\z/ =~ "aa")
    assert_nil(/\Aa?+a\z/ =~ "a")
    assert_nil(/[ab]*+b/ =~ "abab")
    assert_equal "aab", "xaab"[/a*+b/]
    assert_equal "  x", "  x"[/\s*+x/]
    assert_equal "12", /(\d*+)x/.match("12x")[1]
  end

  def test_possessive_repeat_of_a_group
    assert_equal "ababc", "ababc"[/(?:ab)*+c/]
    assert_equal "a%41b@", "a%41b@"[/(?:%\h\h|[a-z])*+@/]
  end

  def test_atomic_group_is_a_plain_group
    assert_equal "abc", "abc"[/(?>ab|a)c/]
    assert_equal "a", /(?>(a))b/.match("ab")[1]
  end

  def test_interval_plus_is_a_repeat_of_the_repeat
    assert_equal 0, /\Aa{1,2}+a\z/ =~ "aa"
    assert_equal "aaaa", "aaaaa"[/a{2}+/]
  end

  def test_nested_repeat
    assert_equal "aaaa", "aaaaa"[Regexp.new("(?:a{2})+")]
    assert_equal "aab", "aab"[Regexp.new("(?:a*)*b")]
    assert_equal "b", "b"[Regexp.new("(?:a*)*b")]
    assert_equal "xxx", "xxx"[Regexp.new("(?:x{1,2}){2}")]
    # CRuby warns of a redundant repeat but takes it
    assert_equal "aa", "aa"[Regexp.new("a**")]
  end
end
