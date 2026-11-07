class RegexpSliceBangTest < Picotest::Test
  def setup
    require 'regexp'
  end

  def test_slice_bang_regexp
    s = "hello world"
    assert_equal "o w", s.slice!(/o\sw/)
    assert_equal "hellorld", s
    assert_nil s.slice!(/x/)
    assert_equal "hellorld", s
  end

  def test_slice_bang_regexp_anchored
    s = "\xEF\xBB\xBFrun App"
    s.slice!(Regexp.new("\\A\xEF\xBB\xBF"))
    assert_equal "run App", s
  end

  def test_slice_bang_regexp_group
    s = "key=value"
    assert_equal "value", s.slice!(/(\w+)=(\w+)/, 2)
    assert_equal "key=", s
    s = "ab"
    assert_nil s.slice!(/a(x)?/, 1)
    assert_equal "ab", s
  end

  def test_slice_bang_regexp_multibyte
    s = "日本語"
    assert_equal "本", s.slice!(/本/)
    assert_equal "日語", s
  end

  def test_slice_bang_regexp_sets_last_match
    "abc".slice!(/b(c)/)
    assert_equal "c", $~[1]
    "abc".slice!(/x/)
    assert_nil $~
  end

  def test_slice_bang_other_forms
    return unless "".respond_to?(:__slice_bang_str) # needs mruby-string-ext
    s = "hello"
    assert_equal "el", s.slice!(1, 2)
    assert_equal "hlo", s
  end
end
