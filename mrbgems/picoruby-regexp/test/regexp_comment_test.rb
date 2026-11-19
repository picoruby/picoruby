class RegexpCommentTest < Picotest::Test
  def test_comment_group_is_not_part_of_the_pattern
    assert_equal "ab", "ab"[/a(?#c)b/]
    assert_equal "a", "a"[/(?#x)a/]
    assert_equal "ab", "ab"[/a(?#c\)d)b/]
  end

  def test_quantifier_after_a_comment_applies_to_the_atom_before_it
    assert_equal "aab", "aab"[/a(?#c)*b/]
  end

  def test_unterminated_comment
    assert_raise(RegexpError) { Regexp.new("a(?#c") }
  end
end
