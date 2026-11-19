class RegexpCallTest < Picotest::Test
  # A recursive \g<n> is unrolled; a search that the copies cut short
  # raises instead of giving an answer that deeper input changes
  def test_recursive_call_nested_too_deep
    re = /\A(\((?:\g<1>)*\))\z/
    assert_true re.match?("(" * 8 + ")" * 8)
    assert_raise(RegexpError) { re.match("(" * 9 + ")" * 9) }
    assert_raise(RegexpError) { re =~ "(" * 12 + ")" * 12 }
    assert_raise(RegexpError) { ("x" + "(" * 12 + ")" * 12).rindex(/(\((?:\g<1>)*\))/) }
    # a later call that runs no search reports nothing
    assert_equal [], "".split(re)
    assert_equal ["a,b"], "a,b".split(re, 1)
  end

  # a call writes out the lookarounds of the group it calls, and the
  # scratch has room for them
  def test_call_of_a_group_with_a_lookaround
    assert_equal 0, /(?=\g<1>)((?=a)a)/ =~ "a"
    assert_raise(RegexpError) { /((?=a\g<1>)?)/.match("a" * 12) }
  end

  # a repeat of a body that compiles to nothing compiles to nothing,
  # however many times it is repeated
  def test_repeat_of_nothing
    assert_true /(?:(?i)(?i)){256}{256}{256}{256}/.match?("a")
    assert_equal "a", /(?<n>a)(){256}{256}{256}{256}/.match("a")[0]
    assert_equal "b", /(?:a{0}){256}{256}{256}{256}b/.match("b")[0]
    assert_true /x(?:){3}y/.match?("xy")
  end

  def test_nested_too_deep
    assert_raise(RegexpError) { Regexp.new("a" + "{1}" * 300) }
  end
end
