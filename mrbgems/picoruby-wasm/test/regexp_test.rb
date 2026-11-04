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

  # The subject string must reach JS RegExp in full, past an embedded NUL.
  def test_match_after_embedded_nul
    assert_equal(2, /b/ =~ "a\x00b")
    assert_equal(true, "a\x00b".match?(/b/))
  end

  # Matched text comes back from JS with its byte length, past an embedded NUL.
  def test_matched_text_keeps_embedded_nul
    md = "xa\x00by".match(/(a.b)/)
    assert_equal("a\x00b", md[0])
    assert_equal(["a\x00b", "a\x00b"], md.to_a)
    assert_equal(["a\x00b"], md.captures)
    assert_equal("a\x00b", md.to_s)
    assert_equal("x3y", "xa\x00by".gsub(/a.b/) { |m| m.size.to_s })
    assert_equal("x<a\x00b>y", "xa\x00by".sub(/a.b/, '<\0>'))
  end

  # The pattern reaches JS RegExp in full, past an embedded NUL.
  def test_pattern_keeps_embedded_nul
    assert_equal(1, Regexp.new("a\x00b") =~ "xa\x00b")
    assert_nil(Regexp.new("a\x00b") =~ "xa")
    assert_equal("x", "xa\x00b".match("a\x00b").pre_match)
  end

  # The source comes back from JS with its byte length, past an embedded NUL.
  def test_source_keeps_embedded_nul
    re = Regexp.new("a\x00b")
    assert_equal("a\x00b", re.source)
    assert_equal("(?:a\x00b)", re.to_s)
    assert_equal("/a\x00b/", re.inspect)
  end
end
