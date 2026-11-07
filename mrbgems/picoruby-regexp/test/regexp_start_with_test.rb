class RegexpStartWithTest < Picotest::Test
  def setup
    require 'regexp'
  end

  def test_start_with_regexp
    assert_true "Hello".start_with?(/[A-Z]/)
    assert_false "hello".start_with?(/[A-Z]/)
    # a match later in the string is not at the start
    assert_false "xa".start_with?(/a/)
    assert_true "abc".start_with?("x", /a/)
    assert_true "abc".start_with?("ab")
    assert_false "abc".start_with?("x", "y")
    assert_true "日本".start_with?("日")
    assert_raise(TypeError) { "abc".start_with?(1) }
  end

  def test_start_with_regexp_sets_last_match
    "abc".start_with?(/a(b)/)
    assert_equal "b", $~[1]
    "abc".start_with?(/x/)
    assert_nil $~
  end
end
