class RegexpExtTest < Picotest::Test
  # ---- String#gsub / #sub with a String pattern match literally ----

  def test_gsub_string_pattern_is_literal
    assert_equal "a-b-c", "a.b.c".gsub(".", "-")
    assert_equal "aXbXc", "a.b.c".gsub(".", "X")
  end

  def test_gsub_string_pattern_with_metachars
    assert_equal "a b", "a(x)b".gsub("(x)", " ")
  end

  def test_sub_string_pattern_is_literal
    assert_equal "a-b.c", "a.b.c".sub(".", "-")
  end

  # ---- String#gsub with a Hash replacement ----

  def test_gsub_hash_replacement
    assert_equal "&lt;x&gt;", "<x>".gsub(/[<>]/, { "<" => "&lt;", ">" => "&gt;" })
  end

  def test_gsub_hash_missing_key_deletes
    assert_equal "ac", "abc".gsub(/[abc]/, { "a" => "a", "c" => "c" })
  end
end
