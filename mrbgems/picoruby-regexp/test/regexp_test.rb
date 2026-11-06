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
    assert_raise(RegexpError) { Regexp.new("(?<n>a)\\k<n>") }
  end

  # ---- Inline options ----

  def test_inline_option_group
    assert_true /(?i:a)b/.match?("Ab")
    assert_false /(?i:a)b/.match?("AB")
    assert_true /(?-i:a)b/i.match?("aB")
    assert_false /(?-i:a)b/i.match?("AB")
    assert_true /(?m:.)/.match?("\n")
    assert_false /(?-m:.)/m.match?("\n")
  end

  def test_inline_option_to_end_of_group
    assert_true /a(?i)b/.match?("aB")
    assert_false /a(?i)b/.match?("AB")
    assert_true /(?:a(?i)b)c/.match?("aBc")
    assert_false /(?:a(?i)b)c/.match?("aBC")
  end

  # ---- Extended mode ----

  def test_extended_ignores_whitespace_and_comments
    re = /
      \A (\d+)   # digits
      \s* - \s*  # a dash
      ([a-z]+) \z
    /x
    md = re.match("12 - ab")
    assert_equal ["12", "ab"], [md[1], md[2]]
    assert_false re.match?("12-ab ")
    assert_true Regexp.new("a b # c", "x").match?("ab")
    assert_true Regexp.new("a b", Regexp::EXTENDED).match?("ab")
  end

  def test_extended_keeps_escaped_whitespace_classes_and_quantifiers
    assert_true /a\ b/x.match?("a b")
    assert_false /a\ b/x.match?("ab")
    assert_true /a\#b/x.match?("a#b")
    assert_true /[ ]/x.match?(" ")
    assert_equal "aaa", /a +/x.match("aaa")[0]
  end

  def test_extended_inline_option
    assert_true /(?x: a b )c/.match?("abc")
    assert_true /a(?x) b c/.match?("abc")
    assert_true /(?-x: a)b/x.match?(" ab")
  end

  def test_interpolated_regexp_keeps_its_options
    inner = /ab/i
    re = /#{inner}c/
    assert_true re.match?("ABc")
    assert_false re.match?("ABC")
    acronyms = /(?=a)b/
    assert_true /^(?:#{acronyms}(?=\b|[A-Z_])|\w)/.match?("x")
  end

  # ---- Word boundaries ----

  def test_word_boundary
    assert_equal 2, (/\bfoo\b/ =~ "a foo b")
    assert_equal nil, (/\bfoo\b/ =~ "afoo b")
    assert_equal 0, (/\bfoo\b/ =~ "foo")
    assert_equal 1, (/\Bfoo/ =~ "afoo")
    assert_equal nil, (/\Bfoo/ =~ "foo")
    assert_equal nil, (/\b/ =~ "  ")
    assert_equal 0, (/\B/ =~ "")
    assert_equal ["foo", "bar"], "foo-bar".scan(/\b\w+\b/)
    assert_equal "Foo Bar", "foo bar".gsub(/\b[a-z]/) { $&.upcase }
  end

  def test_word_boundary_is_ascii
    # \w is ASCII here, so a multibyte letter is not a word character
    assert_equal 1, (/\bé/ =~ "aé")
  end

  # ---- Lookahead and lookbehind ----

  def test_lookahead
    assert_equal "a", /a(?=b)/.match("ab")[0]
    assert_equal nil, /a(?=b)/.match("ac")
    assert_equal "a", /a(?!b)/.match("ac")[0]
    assert_equal nil, /a(?!b)/.match("ab")
    assert_equal nil, /(?=a)b/.match("ab")
  end

  def test_lookbehind
    assert_equal 1, (/(?<=a)b/ =~ "ab")
    assert_equal nil, (/(?<=a)b/ =~ "cb")
    assert_equal 1, (/(?<!a)b/ =~ "cb")
    assert_equal nil, (/(?<!a)b/ =~ "ab")
    assert_equal 0, (/(?<!a)b/ =~ "b")
    assert_equal 2, (/(?<=ab|c)d/ =~ "abd")
    assert_equal 1, (/(?<=ab|c)d/ =~ "cd")
    assert_equal 1, (/(?<=é)b/ =~ "éb")   # a character offset
  end

  def test_lookbehind_must_be_bounded
    assert_raise(RegexpError) { Regexp.new("(?<=a+)b") }
    assert_raise(RegexpError) { Regexp.new("(?<=a*)b") }
    assert_true Regexp.new("(?<=a{1,3})b").match?("aab")
  end

  def test_lookaround_captures
    md = /(?=(a))ab/.match("ab")
    assert_equal "a", md[1]
    assert_equal "ab", md[0]
    md = /(?<=([a-z]))X/.match("aX")
    assert_equal "a", md[1]
    md = /(?!(a))b/.match("b")
    assert_equal nil, md[1]
  end

  def test_lookaround_nesting
    assert_true /a(?=b(?!c))/.match?("abd")
    assert_false /a(?=b(?!c))/.match?("abc")
    assert_raise(RegexpError) { Regexp.new("a(?=b(?=c(?=d(?=e(?=f)))))") }
  end

  def test_lookaround_in_gsub
    assert_equal "foo_bar_baz", "fooBarBaz".gsub(/(?<=[a-z])(?=[A-Z])/, "_").downcase
    assert_equal "HTML_Parser", "HTMLParser".gsub(/(?<=[A-Z])(?=[A-Z][a-z])|(?<=[a-z\d])(?=[A-Z])/, "_")
    assert_equal "1,234,567", "1234567".gsub(/(\d)(?=(\d{3})+$)/) { "#{$1}," }
    assert_equal "a &amp; b &amp; c", "a & b &amp; c".gsub(/&(?!([a-zA-Z]+|(#\d+)|(#[xX][\dA-Fa-f]+));)/, "&amp;")
  end

  def test_end_anchor_alone_matches
    assert_equal 2, ("ab" =~ /$/)
    assert_equal 2, ("ab" =~ /\z/)
    assert_equal "ab!", "ab".sub(/\z/, "!")
    assert_equal "!ab", "ab".sub(/\A/, "!")
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

  # ---- Code escapes and \h ----

  def test_hex_and_unicode_escapes
    assert_true /\x41/.match?("A")
    assert_true /\u0041/.match?("A")
    assert_true /\u{3042}/.match?("あ")
    assert_true /[\x00-\x7f]+/.match?("abc")
    assert_false /[^\x00-\x7f]/.match?("abc")
    assert_true /[^\x00-\x7f]/.match?("é")
    assert_equal "é", /[\u{e9}]/.match("café")[0]
    assert_raise(RegexpError) { Regexp.new("\\xZZ") }
    assert_raise(RegexpError) { Regexp.new("\\u12") }
    assert_raise(RegexpError) { Regexp.new("\\u{110000}") }
  end

  def test_hex_digit_class
    assert_equal "ff", /\h+/.match("xff")[0]
    assert_equal "x", /\H/.match("xff")[0]
    assert_true /[\h]/.match?("A")
  end

  # ---- Named groups ----

  def test_named_groups_by_name
    md = /(?<year>\d{4})-(?<month>\d\d)/.match("on 2024-01-15")
    assert_equal "2024", md[:year]
    assert_equal "01",   md["month"]
    assert_equal "2024", md[1]
    assert_equal 3, md.size
    assert_equal ["year", "month"], md.names
    assert_equal ["year", "month"], md.regexp.names
    assert_equal({ "year" => [1], "month" => [2] }, md.regexp.named_captures)
    assert_equal({ "year" => "2024", "month" => "01" }, md.named_captures)
    assert_equal({ year: "2024", month: "01" }, md.named_captures(symbolize_names: true))
  end

  def test_named_group_offsets
    md = /(?<a>é)(?<b>b)/.match("xéb")
    assert_equal 1, md.begin(:a)
    assert_equal 2, md.end("a")
    assert_equal [1, 3], md.byteoffset(:a)
    assert_equal 2, md.begin(:b)
  end

  def test_plain_groups_do_not_capture_beside_named_ones
    md = /(a)(?<x>b)(c)/.match("abc")
    assert_equal 2, md.size
    assert_equal "b", md[1]
    assert_equal "b", md[:x]
    assert_equal ["b"], md.captures
    assert_equal ["b"], "abc".scan(/(a)(?<x>b)(c)/)[0]
  end

  def test_single_quoted_group_name
    assert_equal "b", /(?'x'b)/.match("ab")[:x]
  end

  def test_undefined_group_name
    md = /(?<x>a)/.match("a")
    assert_raise(IndexError) { md[:y] }
    assert_raise(IndexError) { md.begin("y") }
    assert_raise(IndexError) { "a"[/(?<x>a)/, :y] }
    assert_equal [], /(a)/.names
    assert_equal({}, /(a)/.named_captures)
  end

  def test_same_name_twice
    re = /(?<x>a)|(?<x>b)/
    assert_equal ["x"], re.names
    assert_equal({ "x" => [1, 2] }, re.named_captures)
    assert_equal "b", re.match("b")[:x]
    assert_equal "a", re.match("a")[:x]
    assert_nil(/(?<x>a)?(?<x>c)?/.match("z")[:x])
  end

  def test_named_group_that_did_not_take_part
    md = /(?<a>x)|(?<b>y)/.match("y")
    assert_nil md[:a]
    assert_equal "y", md[:b]
    assert_equal({ "a" => nil, "b" => "y" }, md.named_captures)
  end

  def test_bad_group_names
    assert_raise(RegexpError) { Regexp.new("(?<1a>x)") }
    assert_raise(RegexpError) { Regexp.new("(?<>x)") }
    assert_raise(RegexpError) { Regexp.new("(?<a") }
  end

  def test_string_aref_with_group_name
    assert_equal "15", "2024-01-15"[/(?<y>\d+)-(?<m>\d+)-(?<d>\d+)/, :d]
    assert_equal "15", "2024-01-15"[/(?<y>\d+)-(?<m>\d+)-(?<d>\d+)/, "d"]
    assert_nil "abc"[/(?<y>\d+)/, :y]
  end

  def test_sub_with_named_backreference_in_replacement
    assert_equal "<b>a", "ab".sub(/(?<x>a)(?<y>b)/, '<\k<y>>\k<x>')
    assert_equal "<a", "a".sub(/(?<x>a)/, '<\k<x>')
    assert_equal "ba", "ab".gsub(/(?<x>a)|(?<x>b)/, '\k<x>').reverse
    assert_raise(IndexError) { "a".sub(/(?<x>a)/, '\k<zz>') }
    assert_equal "\\k<x", "a".sub(/(?<x>a)/, '\k<x')
  end

  def test_last_match_by_name
    /(?<word>\w+)/ =~ "hello"
    assert_equal "hello", $~[:word]
    assert_equal "hello", Regexp.last_match[:word]
  end

  def test_deconstruct_keys
    case /(?<y>\d+)-(?<m>\d+)/.match("2024-01")
    in { y:, m: }
      assert_equal "2024", y
      assert_equal "01", m
    end
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

# The String side: sub, gsub, scan, split, index, [], partition,
# Regexp.escape, Regexp.union and $~. A second class, because picotest
# runs one class per generated file and the compiler's jump range does
# not reach across all tests in one file.
class RegexpStringTest < Picotest::Test

  def setup
    require 'regexp'
  end

  # ---- Regexp.escape ----

  def test_escape
    assert_equal "a\\.b\\*c", Regexp.escape("a.b*c")
    assert_equal "\\(x\\)\\[y\\]\\{z\\}\\|\\^\\$\\\\", Regexp.escape("(x)[y]{z}|^$\\")
    assert_equal "a\\nb\\tc d", Regexp.escape("a\nb\tc d")
    assert_true Regexp.new(Regexp.escape("1+1=2?")).match?("is 1+1=2? yes")
    assert_false Regexp.new(Regexp.escape("1+1")).match?("11")
  end

  def test_quote_is_escape
    assert_equal "a\\.b\\*c", Regexp.quote("a.b*c")
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
    # $~ and the group globals describe the current match inside the block
    assert_equal "A_B", "a_b".gsub(/(.)/) { $1.upcase }
    assert_equal "FooBar", "foo_bar".gsub(/(?:^|_)(.)/) { $1.upcase }
    assert_equal "a[b]", "ab".sub(/(b)/) { "[#{$~[1]}]" }
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

  # ---- $~ and its readings ----

  def test_last_match_after_match
    /(\w+) (\w+)/.match("hello world")
    assert_equal "hello world", $~[0]
    assert_equal "hello", $1
    assert_equal "world", $2
    assert_nil $3
    assert_equal "hello world", $&
    assert_equal "world", $+
  end

  def test_last_match_pre_and_post
    "say hello world!" =~ /hello/
    assert_equal "say ", $`
    assert_equal " world!", $'
    assert_equal 4, $~.begin(0)
  end

  def test_last_match_nil_on_failure
    /a/.match("a")
    assert_not_nil $~
    /z/.match("a")
    assert_nil $~
    assert_nil $1
  end

  def test_symbol_matches_as_its_name
    assert_equal 1, (/b/ =~ :ab)
    assert_equal "b", /b/.match(:ab)[0]
    assert_true /b/.match?(:ab)
    assert_false /z/.match?(:ab)
    assert_equal 1, (:ab =~ /b/)
    assert_equal "b", :ab.match(/b/)[0]
    assert_true :ab.match?(/b/)
    assert_true :name=.match?(/[^\]]=\z/)
  end

  def test_last_match_after_string_methods
    "a1b2".gsub(/\d/, "-")
    assert_equal "2", $&
    "x9".scan(/(\d)/)
    assert_equal "9", $1
    "ab".gsub(/b/) { |m| m }
    assert_equal "b", $&
    "hello".index(/l+/)
    assert_equal "ll", $&
    "hello"[/e(l)/]
    assert_equal "l", $1
  end

  def test_match_p_leaves_last_match
    /a/.match("a")
    /z/.match?("a")
    assert_equal "a", $&
  end

  # ---- Regexp.union ----

  def test_union
    re = Regexp.union("a.b", "c*")
    assert_true re.match?("xa.by")
    assert_false re.match?("axb")
    assert_true re.match?("c*")
    assert_equal "dog", Regexp.union(/cat/, /dog/).match("hotdog")[0]
    mixed = Regexp.union(/cat/i, /dog/)
    assert_true mixed.match?("CAT")
    assert_false mixed.match?("DOG")
    assert_equal 0, mixed.options
    assert_equal "cat|(?-mix:d+)", Regexp.union("cat", /d+/).source
    assert_true Regexp.union(["x", "y"]).match?("y")
  end

  def test_union_options
    # each part keeps its options; the union itself has none, as in CRuby
    re = Regexp.union(/a/i, /b/i)
    assert_false re.casefold?
    assert_equal "(?i-mx:a)|(?i-mx:b)", re.source
    assert_true re.match?("A")
    assert_true re.match?("B")
  end

  def test_union_empty_never_matches
    re = Regexp.union
    assert_false re.match?("")
    assert_false re.match?("anything")
  end

  # ---- String#index / rindex ----

  def test_index_with_regexp
    assert_equal 2, "hello".index(/l+/)
    assert_equal 3, "hello".index(/l/, 3)
    assert_nil "hello".index(/z/)
    assert_equal 2, "あいu".index(/u/)
    assert_equal 4, "hello".index(/o/, -1)
  end

  def test_index_with_string_still_works
    assert_equal 2, "hello".index("l")
    assert_equal 3, "hello".index("l", 3)
    assert_nil "hello".index("z")
  end

  def test_rindex_with_regexp
    assert_equal 3, "hello".rindex(/l/)
    assert_equal 2, "hello".rindex(/l/, 2)
    assert_equal 0, "hello".rindex(/h/)
    assert_nil "hello".rindex(/z/)
    assert_equal 3, "a1b2c3".rindex(/\d/, -2)
    assert_equal 5, "hello".rindex(//)
  end

  def test_rindex_with_string
    assert_equal 3, "hello".rindex("l")
    assert_equal 2, "hello".rindex("l", 2)
    assert_equal 3, "a.b.c".rindex(".")
    assert_nil "hello".rindex("z")
    assert_equal 1, "あいあ".rindex("い")
  end

  # ---- String#[] / slice ----

  def test_aref_with_regexp
    assert_equal "ll", "hello"[/l+/]
    assert_equal "l", "hello"[/e(l)/, 1]
    assert_nil "hello"[/z/]
    assert_nil "hello"[/e(l)/, 3]
    assert_equal "ll", "hello".slice(/l+/)
    assert_equal "い", "あいう"[/い/]
  end

  def test_aref_with_other_arguments_still_works
    assert_equal "e", "hello"[1]
    assert_equal "ell", "hello"[1, 3]
    assert_equal "ll", "hello"["ll"]
    assert_nil "hello"["z"]
    assert_equal "o", "hello"[-1]
    assert_equal "ell", "hello".slice(1, 3)
  end

  # ---- String#partition / rpartition ----

  def test_partition_with_regexp
    assert_equal ["he", "ll", "o"], "hello".partition(/l+/)
    assert_equal ["hello", "", ""], "hello".partition(/z/)
    assert_equal ["", "h", "ello"], "hello".partition(/h/)
  end

  def test_rpartition_with_regexp
    assert_equal ["hel", "l", "o"], "hello".rpartition(/l/)
    assert_equal ["", "", "hello"], "hello".rpartition(/z/)
    assert_equal ["a1b", "2", "c"], "a1b2c".rpartition(/\d/)
  end

  def test_partition_with_string
    assert_equal ["a", ".", "b.c"], "a.b.c".partition(".")
    assert_equal ["a.b", ".", "c"], "a.b.c".rpartition(".")
    assert_equal ["abc", "", ""], "abc".partition("x")
    assert_equal ["", "", "abc"], "abc".rpartition("x")
  end

end
