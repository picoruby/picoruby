class IntegerTest < PicoRubyTest
  assert_equal(<<~RUBY, "255")
    p(0b11111111)
  RUBY

  assert_equal(<<~RUBY, "65535")
    p(0b1111111111111111)
  RUBY

  assert_equal(<<~RUBY, "131071")
    p(0b11111111111111111)
  RUBY

  assert_equal(<<~RUBY, "-131071")
    p(-0b11111111111111111)
  RUBY

  assert_equal(<<~RUBY, "2097151")
    p(0o7777777)
  RUBY

  assert_equal(<<~RUBY, "-2097151")
    p(-0o7777777)
  RUBY

  assert_equal(<<~RUBY, "65535")
    p(0xFFFF)
  RUBY

  assert_equal(<<~RUBY, "1048575")
    p(0xFFFFF)
  RUBY

  assert_equal(<<~RUBY, "-1048575")
    p(-0xFFFFF)
  RUBY

  assert_equal(<<~RUBY, "70000")
    p(-70000.abs)
  RUBY

  assert_equal(<<~RUBY, "7000")
    p(7000.001.to_i)
  RUBY

  assert_equal(<<~RUBY, "-7000")
    p(-7000.001.to_i)
  RUBY
end
