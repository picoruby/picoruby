class RegexpPosixTest < Picotest::Test
  def test_posix_bracket_classes
    assert_equal "ab", "ab1"[/[[:alpha:]]+/]
    assert_equal "1", "a1"[/[[:^alpha:]]/]
    assert_equal "a_1", "a_1-"[/[[:alnum:]_]+/]
    assert_equal "!", "a!"[/[[:punct:]]/]
    assert_equal "B3", "aB3c"[/[[:upper:][:digit:]]+/]
    assert_equal "ab", "  ab "[/[^[:space:]]+/]
    assert_raise(RegexpError) { Regexp.new("[[:foo:]]") }
  end

  def test_negated_escapes_in_a_class
    assert_equal "!", "a!"[/[\W]/]
    assert_equal "x", "1x"[/[\D]/]
    assert_equal "ab", "1ab2"[/[^\W\d]+/]
  end

  def test_octal_escapes
    assert_equal "a", "a"[/[\0-\177]/]
    assert_equal "é", "xé"[/[^\0-\177]/]
    assert_equal "\n", "a\nb"[/\012/]
    assert_equal 1, (/\177/ =~ "a\x7f")
    assert_raise(RegexpError) { Regexp.new("[\\200]") }
  end

  def test_local_name_pattern
    re = /\A(?![A-Z0-9])(?:[[:alnum:]_]|[^\0-\177])+\z/
    assert_true re.match?("local_var1")
    assert_true re.match?("名前")
    assert_false re.match?("Foo")
  end
end
