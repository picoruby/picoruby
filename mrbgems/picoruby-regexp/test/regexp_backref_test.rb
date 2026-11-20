class RegexpBackrefTest < Picotest::Test
  def test_numbered_backreference
    assert_equal ["aa", "a"], /(a)\1/.match("xaa").to_a
    assert_nil(/(a)\1/ =~ "ab")
    assert_equal "hello hello", "hello hello world"[/(\w+)\s\1/]
    assert_equal "yyy", "xyyyz"[/(.)\1{2}/]
  end

  def test_named_backreference
    md = /(?<q>['"]).*?\k<q>/.match(%q{say "it's" ok})
    assert_equal %q{"it's"}, md[0]
    assert_equal '"', md[:q]
    assert_equal "aa", "aa"[/(?<q>a)\k'q'/]
    assert_equal "abb", "abb"[/(a)(b)\k<-1>/]
  end

  def test_unset_group_matches_nothing
    assert_nil(/(a)?b\1/ =~ "b")
    assert_nil(/\1(a)/ =~ "aa")
  end

  def test_case_folded_backreference
    assert_equal "aA", "aA"[/(a)\1/i]
  end

  def test_backreference_with_lookaround
    assert_equal "a", "aa"[/(a)(?=\1)/]
    assert_equal "ba", /(?<=(a))b\1/.match("aba")[0]
  end

  def test_backreference_in_scan_and_gsub
    assert_equal [["a"], ["b"]], "aabxbb".scan(/(\w)\1/)
    assert_equal "a-b-", "aabb".gsub(/(\w)\1/, "\\1-")
  end

  def test_join_table_name
    m = /^(.*[_.])(.+)\0\1(.+)/.match("posts_tags\0posts_users")
    assert_equal ["posts_", "tags", "users"], m.captures
  end

  def test_backreference_errors
    assert_raise(RegexpError) { Regexp.new("(a)\\2") }
    assert_raise(RegexpError) { Regexp.new("(?<n>a)\\1") }
    assert_raise(RegexpError) { Regexp.new("(?<n>a)\\k<m>") }
    # \k<n> to a group that does not exist, and a numbered one beside a
    # named group
    assert_raise(RegexpError) { Regexp.new("\\k<2>") }
    assert_raise(RegexpError) { Regexp.new("(a)(?<n>b)\\k<-1>") }
    # a bare \k is a k
    assert_equal "k", "k"[Regexp.new("\\k")]
  end

  def test_backtracking_limit_raises
    re = Regexp.new("(x+x+)+y\\1")
    assert_raise(RegexpError) { re.match("x" * 40) }
    assert_raise(RegexpError) { "x" * 40 =~ re }
    assert_raise(RegexpError) { ("x" * 40).rindex(re) }
    # a later call that runs no search reports nothing
    assert_equal [], "".split(re)
    assert_equal ["abc"], "abc".split(re, 1)
  end

  # a recursive call cut short on the backtracker raises as on the Pike VM
  def test_recursive_call_nested_too_deep
    re = /(\((?:\g<1>)*\))\1/
    assert_equal "(())(())", re.match("(())(())")[0]
    assert_raise(RegexpError) { re.match("(" * 9 + ")" * 9) }
  end
end
