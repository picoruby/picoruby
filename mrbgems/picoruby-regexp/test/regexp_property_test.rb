class RegexpPropertyTest < Picotest::Test
  def test_property
    assert_equal "abc", "ABabcD"[/\p{Lower}+/]
    assert_equal "AB", "ABabcD"[/\p{Upper}+/]
    assert_equal "12", "ab12"[/\p{Digit}+/]
    assert_equal "a_1", "-a_1-"[/\p{Word}+/]
  end

  def test_property_name_in_any_case
    assert_equal "abc", "ABabcD"[/\p{lower}+/]
    assert_equal "abc", "ABabcD"[/\p{LOWER}+/]
  end

  def test_negated_property
    assert_equal "AB", "abABc"[/\P{Lower}+/]
    assert_equal "AB", "abABc"[/\p{^Lower}+/]
    assert_equal "ab", "abABc"[/\P{^Lower}+/]
  end

  def test_property_in_a_class
    assert_equal "aB3", "-aB3-"[/[\p{Alpha}\p{Digit}]+/]
    assert_equal "-", "aB-"[/[\P{Alnum}]/]
  end

  def test_property_after_a_lookbehind
    re = /\b(?<!\w['’`()])\p{Lower}/
    assert_equal "Hello World", "hello world".gsub(re) { |m| m.upcase }
  end

  def test_unknown_property
    assert_raise(RegexpError) { Regexp.new("\\p{Foo}") }
    assert_raise(RegexpError) { Regexp.new("\\p{Lower") }
  end
end
