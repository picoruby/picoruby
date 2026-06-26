class RegexpExtTest < Picotest::Test
  def test_gsub_with_string_pattern_treats_pattern_as_literal
    assert_equal("axb", "a.b".gsub(".", "x"))
    assert_equal("a.b", "a.b".gsub("z", "x"))
    assert_equal("axb", "a[b".gsub("[", "x"))
  end

  def test_sub_with_string_pattern_treats_pattern_as_literal
    assert_equal("axb.c", "a.b.c".sub(".", "x"))
    assert_equal("a.b.c", "a.b.c".sub("z", "x"))
    assert_equal("axb", "a[b".sub("[", "x"))
  end

  def test_gsub_block_raises_when_receiver_is_modified
    source = "ab" * 1_000

    assert_raise(RuntimeError) do
      source.gsub(/./) do
        source.replace("x" * 100_000)
        "z"
      end
    end
  end
end
