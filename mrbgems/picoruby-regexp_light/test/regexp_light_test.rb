class RegexpLightTest < Picotest::Test

  def setup
    require 'regexp_light'
  end

  # ---- Regexp.compile / Regexp.new ----

  def test_compile_basic
    re = Regexp.compile("hello")
    assert_true  re.match?("hello world")
    assert_false re.match?("world")
  end

  def test_new_basic
    re = Regexp.new("world")
    assert_true  re.match?("hello world")
    assert_false re.match?("hello")
  end

  # ---- Literal syntax ----

  def test_literal_basic
    re = /hello/
    assert_true  re.match?("hello world")
    assert_false re.match?("world")
  end

  def test_literal_with_i_flag
    re = /hello/i
    assert_equal "hello", re.source
    assert_true  re.casefold?
    assert_equal 1, re.options
  end

  # ---- Regexp#match ----

  def test_match_returns_match_data
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal "hello world", md[0]
    assert_equal "hello",       md[1]
    assert_equal "world",       md[2]
  end

  def test_match_returns_nil_on_no_match
    md = /xyz/.match("hello world")
    assert_nil md
  end

  # ---- Regexp#match? ----

  def test_match_p_true
    assert_true /\d+/.match?("abc123")
  end

  def test_match_p_false
    assert_false /\d+/.match?("abcxyz")
  end

  # ---- Regexp#=~ ----

  def test_match_op_returns_index
    assert_equal 3, /\d+/ =~ "abc123def"
  end

  def test_match_op_returns_nil_on_no_match
    assert_nil(/\d+/ =~ "abcdef")
  end

  # ---- Anchors (\A and \z converted to ^ and $) ----

  def test_anchor_begin
    re = /\Ahello/
    assert_true  re.match?("hello world")
    assert_false re.match?("say hello")
  end

  def test_anchor_end
    re = /world\z/
    assert_true  re.match?("hello world")
    assert_false re.match?("world domination")
  end

  def test_anchor_begin_and_end
    re = /\Ahello\z/
    assert_true  re.match?("hello")
    assert_false re.match?("hello world")
    assert_false re.match?("say hello")
  end

  # ---- Character classes ----

  def test_char_class_d
    md = /\d+/.match("abc 42 def")
    assert_equal "42", md[0]
  end

  def test_char_class_w
    md = /\w+/.match("  hello  ")
    assert_equal "hello", md[0]
  end

  def test_char_class_s
    md = /\s+/.match("hello world")
    assert_equal " ", md[0]
  end

  def test_bracket_class
    md = /[aeiou]+/.match("beautiful")
    assert_equal "eau", md[0]
  end

  def test_bracket_range
    md = /[a-z]+/.match("123abc456")
    assert_equal "abc", md[0]
  end

  # ---- Quantifiers ----

  def test_star
    md = /ab*c/.match("ac")
    assert_equal "ac", md[0]
    md2 = /ab*c/.match("abbc")
    assert_equal "abbc", md2[0]
  end

  def test_plus
    md = /ab+c/.match("ac")
    assert_nil md
    md2 = /ab+c/.match("abbc")
    assert_equal "abbc", md2[0]
  end

  def test_question
    md = /colou?r/.match("color")
    assert_equal "color", md[0]
    md2 = /colou?r/.match("colour")
    assert_equal "colour", md2[0]
  end

  def test_dot
    md = /h.llo/.match("hello")
    assert_equal "hello", md[0]
    md2 = /h.llo/.match("hxllo")
    assert_equal "hxllo", md2[0]
  end

  # ---- Repeat quantifiers {n}, {n,m}, {n,} ----

  def test_repeat_exact
    md = /a{3}/.match("xaaay")
    assert_equal "aaa", md[0]
    assert_nil /a{3}/.match("aa")
  end

  def test_repeat_range
    md = /a{2,4}/.match("aaaaa")
    assert_equal "aaaa", md[0]
    md2 = /a{2,4}/.match("aa")
    assert_equal "aa", md2[0]
    assert_nil /a{2,4}/.match("a")
  end

  def test_repeat_unbounded
    md = /a{2,}b/.match("aaaab")
    assert_equal "aaaab", md[0]
    md2 = /a{2,}b/.match("aab")
    assert_equal "aab", md2[0]
    assert_nil /a{2,}b/.match("ab")
  end

  def test_repeat_with_bracket
    md = /[0-9]{3}/.match("abc123def")
    assert_equal "123", md[0]
  end

  def test_repeat_with_char_class
    md = /\d{4}-\d{2}-\d{2}/.match("Date: 2024-01-15!")
    assert_equal "2024-01-15", md[0]
  end

  def test_repeat_group_exact
    md = /(ab){3}/.match("xabababx")
    assert_equal "ababab", md[0]
    assert_nil /(ab){3}/.match("abab")
  end

  def test_repeat_group_range
    md = /(ab){2,3}c/.match("abababc")
    assert_equal "abababc", md[0]
    md2 = /(ab){2,3}c/.match("ababc")
    assert_equal "ababc", md2[0]
  end

  def test_repeat_group_unbounded
    md = /(ab){2,}c/.match("abababababc")
    assert_equal "abababababc", md[0]
  end

  # ---- Capture groups ----

  def test_capture_groups
    md = /(\d\d\d\d)-(\d\d)-(\d\d)/.match("Date: 2024-01-15")
    assert_equal "2024-01-15", md[0]
    assert_equal "2024",       md[1]
    assert_equal "01",         md[2]
    assert_equal "15",         md[3]
  end

  def test_captures_method
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal ["hello", "world"], md.captures
  end

  def test_no_captures
    md = /hello/.match("hello world")
    assert_equal [], md.captures
  end

  # ---- MatchData methods ----

  def test_match_data_to_a
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal ["hello world", "hello", "world"], md.to_a
  end

  def test_match_data_length
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal 3, md.length
    assert_equal 3, md.size
  end

  def test_match_data_string
    md = /hello/.match("hello world")
    assert_equal "hello world", md.string
  end

  def test_match_data_pre_post_match
    md = /world/.match("hello world goodbye")
    assert_equal "hello ", md.pre_match
    assert_equal " goodbye", md.post_match
  end

  def test_match_data_begin_end
    md = /(\d+)/.match("abc 42 def")
    assert_equal 4, md.begin(0)
    assert_equal 6, md.end(0)
    assert_equal 4, md.begin(1)
    assert_equal 6, md.end(1)
  end

  def test_match_data_negative_index
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal "world", md[-1]
    assert_equal "hello", md[-2]
  end

  # ---- Regexp#source, inspect, to_s ----

  def test_source
    re = /hello\d+/
    assert_equal "hello\\d+", re.source
  end

  def test_inspect_no_flags
    re = /hello/
    assert_equal "/hello/", re.inspect
  end

  def test_inspect_with_flags
    re = /hello/i
    assert_equal "/hello/i", re.inspect
  end

  # ---- String extensions ----

  def test_string_match
    md = "hello world".match(/(\w+)\s(\w+)/)
    assert_equal "hello world", md[0]
    assert_equal "hello",       md[1]
  end

  def test_string_match_p
    assert_true  "hello123".match?(/\d+/)
    assert_false "helloabc".match?(/\d+/)
  end

  def test_string_match_op
    assert_equal 4, "abc 42 def" =~ /\d+/
    assert_nil("abcdef" =~ /\d+/)
  end

  def test_string_match_with_string_pattern
    md = "hello world".match("world")
    assert_equal "world", md[0]
  end

  # ---- Regexp constants ----

  def test_constants
    assert_equal 1, Regexp::IGNORECASE
    assert_equal 2, Regexp::EXTENDED
    assert_equal 4, Regexp::MULTILINE
  end

  # ---- Route constraint use-case ----

  def test_numeric_id_constraint
    re = /\d+/
    assert_true  re.match?("123")
    assert_false re.match?("abc")
    assert_true  re.match?("12abc")
  end

  def test_exact_numeric_constraint
    re = /\A\d+\z/
    assert_true  re.match?("123")
    assert_false re.match?("abc")
    assert_false re.match?("12abc")
  end

end
