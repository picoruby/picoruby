class RegexpTest < Picotest::Test

  def setup
    require 'regexp'
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

  def test_new_with_string_options
    re = Regexp.new("hello", "i")
    assert_true re.match?("HELLO")
    assert_equal 1, re.options
  end

  def test_new_with_integer_options
    re = Regexp.new("a.b", Regexp::MULTILINE | Regexp::IGNORECASE)
    assert_true re.match?("A\nB")
    assert_equal 5, re.options
  end

  def test_new_from_regexp
    re = Regexp.new(/ab+/i)
    assert_equal "ab+", re.source
    assert_true re.casefold?
  end

  def test_compile_error_raises_regexp_error
    assert_raise(RegexpError) { Regexp.new("(") }
    assert_raise(RegexpError) { Regexp.new("[a") }
    assert_raise(RegexpError) { Regexp.new("a**") }
    assert_raise(RegexpError) { Regexp.new("a{3,2}") }
  end

  def test_unsupported_syntax_raises_regexp_error
    assert_raise(RegexpError) { Regexp.new("(a)\\1") }
    assert_raise(RegexpError) { Regexp.new("\\bx") }
    assert_raise(RegexpError) { Regexp.new("(?=a)") }
    assert_raise(RegexpError) { Regexp.new("(?<n>a)") }
  end

  def test_unknown_option_raises
    assert_raise(ArgumentError) { Regexp.new("a", "q") }
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
    assert_true  re.match?("HeLLo")
  end

  def test_literal_with_m_flag
    assert_false(/a.b/.match?("a\nb"))
    assert_true(/a.b/m.match?("a\nb"))
    assert_equal 4, /a/m.options
  end

  # ---- Regexp#match ----

  def test_match_returns_match_data
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal "hello world", md[0]
    assert_equal "hello",       md[1]
    assert_equal "world",       md[2]
  end

  def test_match_returns_nil_on_no_match
    assert_nil(/xyz/.match("hello world"))
  end

  def test_match_with_pos
    md = /\d+/.match("a1 b22 c333", 3)
    assert_equal "22", md[0]
    assert_equal 4, md.begin(0)
    assert_nil(/a/.match("abc", 1))
    assert_nil(/a/.match("abc", 4))
  end

  def test_match_with_negative_pos
    md = /\d/.match("1a2b3", -2)
    assert_equal "3", md[0]
  end

  def test_match_nil_returns_nil
    assert_nil(/a/.match(nil))
  end

  # ---- Regexp#match? ----

  def test_match_p_true
    assert_true(/\d+/.match?("abc123"))
  end

  def test_match_p_false
    assert_false(/\d+/.match?("abcxyz"))
  end

  def test_match_p_with_pos
    assert_true(/\d/.match?("ab1", 2))
    assert_false(/a/.match?("abc", 1))
  end

  # ---- Regexp#=~ ----

  def test_match_op_returns_index
    assert_equal 3, /\d+/ =~ "abc123def"
  end

  def test_match_op_returns_nil_on_no_match
    assert_nil(/\d+/ =~ "abcdef")
  end

  # ---- Anchors ----

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

  def test_caret_and_dollar_are_string_anchors
    assert_true(/^abc$/.match?("abc"))
    assert_false(/^abc$/.match?("xabc"))
    assert_true(/^$/.match?(""))
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

  def test_char_class_negated
    md = /\D+/.match("123abc456")
    assert_equal "abc", md[0]
    md = /\S+/.match("  xy  ")
    assert_equal "xy", md[0]
  end

  def test_bracket_class
    md = /[aeiou]+/.match("beautiful")
    assert_equal "eau", md[0]
  end

  def test_bracket_range
    md = /[a-z]+/.match("123abc456")
    assert_equal "abc", md[0]
  end

  def test_bracket_negated
    md = /[^a-z ]+/.match("abc X! y")
    assert_equal "X!", md[0]
  end

  # ---- Quantifiers ----

  def test_star
    assert_equal "ac", /ab*c/.match("ac")[0]
    assert_equal "abbc", /ab*c/.match("abbc")[0]
  end

  def test_plus
    assert_nil(/ab+c/.match("ac"))
    assert_equal "abbc", /ab+c/.match("abbc")[0]
  end

  def test_question
    assert_equal "color", /colou?r/.match("color")[0]
    assert_equal "colour", /colou?r/.match("colour")[0]
  end

  def test_dot
    assert_equal "hello", /h.llo/.match("hello")[0]
    assert_equal "hxllo", /h.llo/.match("hxllo")[0]
  end

  def test_lazy_quantifiers
    assert_equal "<a>", /<.*?>/.match("<a><b>")[0]
    assert_equal "<a><b>", /<.*>/.match("<a><b>")[0]
    assert_equal "a", /a+?/.match("aaa")[0]
    assert_equal "aa", /a{2,4}?/.match("aaaa")[0]
  end

  # ---- Alternation ----

  def test_alternation
    md = /cat|dog/.match("hotdog")
    assert_equal "dog", md[0]
    assert_equal "a", /a|ab/.match("ab")[0]
    assert_equal "abcd", /(a|b|c)+d/.match("xabcdy")[0]
  end

  # ---- Repeat quantifiers {n}, {n,m}, {n,} ----

  def test_repeat_exact
    assert_equal "aaa", /a{3}/.match("xaaay")[0]
    assert_nil(/a{3}/.match("aa"))
  end

  def test_repeat_range
    assert_equal "aaaa", /a{2,4}/.match("aaaaa")[0]
    assert_equal "aa", /a{2,4}/.match("aa")[0]
    assert_nil(/a{2,4}/.match("a"))
  end

  def test_repeat_unbounded
    assert_equal "aaaab", /a{2,}b/.match("aaaab")[0]
    assert_equal "aab", /a{2,}b/.match("aab")[0]
    assert_nil(/a{2,}b/.match("ab"))
  end

  def test_repeat_with_bracket
    assert_equal "123", /[0-9]{3}/.match("abc123def")[0]
  end

  def test_repeat_with_char_class
    assert_equal "2024-01-15", /\d{4}-\d{2}-\d{2}/.match("Date: 2024-01-15!")[0]
  end

  def test_repeat_group_exact
    assert_equal "ababab", /(ab){3}/.match("xabababx")[0]
    assert_nil(/(ab){3}/.match("abab"))
  end

  def test_repeat_group_range
    assert_equal "abababc", /(ab){2,3}c/.match("abababc")[0]
    assert_equal "ababc", /(ab){2,3}c/.match("ababc")[0]
  end

  def test_repeat_group_unbounded
    assert_equal "abababababc", /(ab){2,}c/.match("abababababc")[0]
  end

  def test_literal_brace
    assert_equal "a{", /a{/.match("a{")[0]
    assert_equal "{3}", /\{3\}/.match("x{3}")[0]
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

  def test_group_that_did_not_take_part
    md = /(a)|(b)/.match("b")
    assert_nil md[1]
    assert_equal "b", md[2]
    assert_equal [nil, "b"], md.captures
    assert_nil md.begin(1)
  end

  def test_non_capturing_group
    md = /(?:ab)+(c)/.match("ababc")
    assert_equal 2, md.size
    assert_equal "c", md[1]
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
    # mruby/c has no frozen?
    assert_true md.string.frozen? if picoruby?
  end

  def test_match_data_regexp
    re = /hello/
    md = re.match("hello world")
    assert_equal "hello", md.regexp.source
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

  def test_match_data_begin_end_out_of_range
    md = /(a)/.match("a")
    assert_raise(IndexError) { md.begin(2) }
    assert_raise(IndexError) { md.end(-3) }
  end

  def test_match_data_negative_index
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal "world", md[-1]
    assert_equal "hello", md[-2]
    assert_nil md[3]
  end

  def test_match_data_to_s_and_inspect
    md = /(\w+)\s(\w+)/.match("hello world")
    assert_equal "hello world", md.to_s
    assert_equal '#<MatchData "hello world" 1:"hello" 2:"world">', md.inspect
    assert_equal '#<MatchData "b" 1:nil 2:"b">', /(a)|(b)/.match("b").inspect
  end

  # ---- UTF-8 ----

  def test_utf8_dot_matches_one_character
    md = /./.match("あb")
    assert_equal "あ", md[0]
    assert_equal 0, md.begin(0)
    assert_equal 1, md.end(0)
  end

  def test_utf8_offsets_are_characters
    md = /b/.match("あいb")
    assert_equal 2, md.begin(0)
    assert_equal 3, md.end(0)
    assert_equal 2, "あいb" =~ /b/
    assert_equal "あい", md.pre_match
  end

  def test_utf8_pos_is_characters
    md = /./.match("あいう", 1)
    assert_equal "い", md[0]
    assert_equal 1, md.begin(0)
  end

  def test_utf8_class_range
    assert_equal "い", /[い-う]+/.match("あい")[0]
    assert_equal "日本", /日本/.match("私は日本人")[0]
  end

  # ---- Regexp#source, inspect, to_s ----

  def test_source
    re = /hello\d+/
    assert_equal "hello\\d+", re.source
    assert_true re.source.frozen? if picoruby?
  end

  def test_inspect_no_flags
    assert_equal "/hello/", /hello/.inspect
  end

  def test_inspect_with_flags
    assert_equal "/hello/i", /hello/i.inspect
    assert_equal "/hello/mi", /hello/mi.inspect
  end

  def test_to_s
    assert_equal "(?-mix:hello)", /hello/.to_s
    assert_equal "(?i-mx:hello)", /hello/i.to_s
    assert_equal "(?mi-x:hello)", /hello/mi.to_s
  end

  # ---- String extensions ----

  def test_string_match
    md = "hello world".match(/(\w+)\s(\w+)/)
    assert_equal "hello world", md[0]
    assert_equal "hello",       md[1]
  end

  def test_string_match_with_pos
    md = "a1 b2".match(/\d/, 2)
    assert_equal "2", md[0]
  end

  def test_string_match_p
    assert_true  "hello123".match?(/\d+/)
    assert_false "helloabc".match?(/\d+/)
    assert_true  "ab1".match?(/\d/, 2)
  end

  def test_string_match_op
    assert_equal 4, "abc 42 def" =~ /\d+/
    assert_nil("abcdef" =~ /\d+/)
  end

  def test_string_match_with_string_pattern
    md = "hello world".match("world")
    assert_equal "world", md[0]
    assert_true "a.c".match?("a\\.c")
  end

  def test_string_match_with_bad_type
    assert_raise(TypeError) { "abc".match(1) }
  end

  # ---- Regexp#=== (for case-when) ----

  def test_triple_equal_match
    assert_true(/hello/ === "hello world")
    assert_false(/hello/ === "world")
  end

  def test_triple_equal_non_string_is_false
    assert_false(/1/ === 1)
    assert_false(/a/ === nil)
    assert_true(/a/ === :a)
  end

  def test_triple_equal_in_case_when
    result = case "hello123"
    when /\A\d+\z/ then :digits
    when /[a-z]+/  then :has_lower
    else                :other
    end
    assert_equal :has_lower, result
  end

  # ---- Regexp constants ----

  def test_constants
    assert_equal 1, Regexp::IGNORECASE
    assert_equal 2, Regexp::EXTENDED
    assert_equal 4, Regexp::MULTILINE
  end

  # ---- Linear time ----

  def test_pathological_pattern_finishes
    s = "a" * 40
    assert_false(/(a*)*b/.match?(s))
    assert_false(/(a|aa)+$/.match?(s + "c"))
  end

  # ---- Regexp.escape ----

  def test_escape
    assert_equal "a\\.b\\*c", Regexp.escape("a.b*c")
    assert_equal "\\(x\\)\\[y\\]\\{z\\}\\|\\^\\$\\\\", Regexp.escape("(x)[y]{z}|^$\\")
    assert_equal "a\\nb\\tc d", Regexp.escape("a\nb\tc d")
    assert_true Regexp.new(Regexp.escape("1+1=2?")).match?("is 1+1=2? yes")
    assert_false Regexp.new(Regexp.escape("1+1")).match?("11")
  end

  # ---- String#sub / gsub with a String replacement ----

  def test_sub_with_regexp
    assert_equal "hexlo", "hello".sub(/l/, "x")
    assert_equal "hello", "hello".sub(/z/, "x")
    assert_equal "b-c", "abc".sub(/a(.)/, "\\1-")
  end

  def test_gsub_with_regexp
    assert_equal "hexxo", "hello".gsub(/l/, "x")
    assert_equal "h_ll_", "hello".gsub(/[aeiou]/, "_")
    assert_equal "hello", "hello".gsub(/z/, "x")
  end

  def test_gsub_backreferences
    assert_equal "b:a", "a:b".gsub(/(\w):(\w)/, "\\2:\\1")
    assert_equal "[hello] [world]", "hello world".gsub(/\w+/, "[\\0]")
    assert_equal "<hi>", "hi".gsub(/\w+/, "<\\&>")
    assert_equal "a\\b", "ab".sub(/a/, "a\\\\")
    assert_equal "x\\qy", "xy".sub(/x/, "x\\q")
    assert_equal "-b", "ab".sub(/(a)|(b)/, "-\\2")
  end

  def test_gsub_empty_matches
    assert_equal "-a-b-c-", "abc".gsub(/x*/, "-")
    assert_equal "--", "aaa".gsub(/a*/, "-")
    assert_equal "-あ-い-", "あい".gsub(//, "-")
  end

  def test_gsub_with_string_pattern_is_literal
    assert_equal "a-b", "a.b".gsub(".", "-")
    assert_equal "1+1", "1*1".gsub("*", "+")
    assert_equal "&lt;b&gt;", "<b>".gsub("<", "&lt;").gsub(">", "&gt;")
    assert_equal "a''b", "a'b".gsub("'", "''")
    assert_equal "x-x", "xax".sub("a", "-")
    assert_equal "[a]", "a".gsub("a", "[\\0]")
  end

  def test_sub_gsub_utf8
    assert_equal "私は日本人", "私はRuby人".sub(/Ruby/, "日本")
    assert_equal "a_b", "aあb".gsub(/あ/, "_")
    assert_equal "-x-", "あxい".gsub(/[あい]/, "-")
  end

  # ---- String#sub / gsub with a block or a Hash ----

  def test_gsub_with_block
    assert_equal "HELLO world", "hello world".gsub(/hello/) { |m| m.upcase }
    assert_equal "1 4 9", "1 2 3".gsub(/\d/) { |d| (d.to_i * d.to_i).to_s }
    assert_equal "hello", "hello".gsub(/z/) { |m| "x" }
    assert_equal "-a-b-", "ab".gsub(/x*/) { "-" }
  end

  def test_sub_with_block
    assert_equal "Hello world", "hello world".sub(/h/) { |m| m.upcase }
  end

  def test_gsub_with_hash
    assert_equal "1 2 c", "a b c".gsub(/[ab]/, { "a" => "1", "b" => "2" })
    assert_equal "x  z", "x y z".gsub(/y/, {})
  end

  def test_gsub_string_modified_raises
    s = "hello"
    assert_raise(RuntimeError) { s.gsub(/l/) { |m| s << "!"; m } }
  end

  def test_gsub_bad_arguments
    assert_raise(ArgumentError) { "a".gsub(/a/) }
    assert_raise(TypeError) { "a".gsub(/a/, 1) }
    assert_raise(TypeError) { "a".gsub(1, "b") }
  end

  # ---- String#sub! / gsub! ----

  def test_sub_bang
    s = "hello"
    assert_equal "hexlo", s.sub!(/l/, "x")
    assert_equal "hexlo", s
    assert_nil s.sub!(/z/, "x")
    assert_equal "hexlo", s
  end

  def test_gsub_bang
    s = "hello"
    assert_equal "hexxo", s.gsub!(/l/, "x")
    assert_equal "hexxo", s
    assert_nil s.gsub!(/l/, "x")
    s2 = "aaa"
    assert_equal "aaa", s2.gsub!(/a/, "a")
    s3 = "hello"
    assert_equal "HELLO", s3.gsub!(/./) { |c| c.upcase }
  end

  # ---- String#scan ----

  def test_scan_without_groups
    assert_equal ["1", "22", "333"], "a1 b22 c333".scan(/\d+/)
    assert_equal [], "abc".scan(/\d/)
    assert_equal ["", "", "", ""], "abc".scan(//)
    assert_equal ["あ", "い"], "あxい".scan(/[^x]/)
  end

  def test_scan_with_groups
    assert_equal [["a", "1"], ["b", "2"]], "a1 b2".scan(/(\w)(\d)/)
    assert_equal [["a", nil], [nil, "1"]], "a1".scan(/(a)|(1)/)
  end

  def test_scan_with_block
    seen = []
    ret = "a1 b2".scan(/\d/) { |d| seen << d }
    assert_equal ["1", "2"], seen
    assert_equal "a1 b2", ret
  end

  def test_scan_with_string_pattern
    assert_equal [".", "."], "a.b.c".scan(".")
  end

  # ---- String#split ----

  def test_split_with_regexp
    assert_equal ["a", "b", "c"], "a, b,c".split(/,\s*/)
    assert_equal ["a", "b", "c"], "a1b22c".split(/\d+/)
    assert_equal ["abc"], "abc".split(/x/)
    assert_equal [], "".split(/,/)
  end

  def test_split_empty_regexp_and_edges
    assert_equal ["a", "b", "c"], "abc".split(//)
    assert_equal ["あ", "い"], "あい".split(//)
    assert_equal ["", "a", "b"], ",a,b".split(/,/)
    assert_equal ["a", "b"], "a,b,,".split(/,/)
    assert_equal ["a", "b", "", ""], "a,b,,".split(/,/, -1)
  end

  def test_split_with_limit
    assert_equal ["a", "b,c"], "a,b,c".split(/,/, 2)
    assert_equal ["a,b,c"], "a,b,c".split(/,/, 1)
    assert_equal ["a", "b", "c"], "a,b,c".split(/,/, 10)
  end

  def test_split_with_captures
    assert_equal ["a", "1", "b", "2", "c"], "a1b2c".split(/(\d)/)
    assert_equal ["a", "1", "b"], "a1b".split(/(\d)|(x)/)
  end

  def test_split_with_string_still_works
    assert_equal ["a", "b", "c"], "a,b,c".split(",")
    assert_equal ["a", "b"], "a b".split(" ")
    assert_equal ["a", "b"], "a b".split
    assert_equal ["a", "b,c"], "a,b,c".split(",", 2)
  end

  # ---- MatchData#byteoffset ----

  def test_byteoffset
    md = /b/.match("あb")
    assert_equal [3, 4], md.byteoffset(0)
    assert_equal 1, md.begin(0)
    assert_nil(/(a)|(b)/.match("b").byteoffset(1))
    assert_raise(IndexError) { md.byteoffset(5) }
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
