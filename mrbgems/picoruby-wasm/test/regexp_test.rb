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

  def test_gsub_with_hash_replacement
    assert_equal("&lt;x&gt;", "<x>".gsub(/[<>]/, { "<" => "&lt;", ">" => "&gt;" }))
    assert_equal("Ax>", "<x>".gsub("<", { "<" => "A" }))
    assert_equal("ac", "abc".gsub(/[ab]/, { "a" => "a" }))
  end

  def test_sub_with_hash_replacement
    assert_equal("&lt;x>", "<x>".sub(/[<>]/, { "<" => "&lt;", ">" => "&gt;" }))
    assert_equal("Ax>", "<x>".sub("<", { "<" => "A" }))
  end

  def test_gsub_hash_default_block_raises_when_receiver_is_modified
    source = "ab" * 1_000
    replacements = Hash.new do |_hash, _key|
      source.replace("x" * 100_000)
      "z"
    end

    assert_raise(RuntimeError) do
      source.gsub(/./, replacements)
    end
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
