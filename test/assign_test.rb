class AssignTest < PicoRubyTest
  assert_equal(<<~RUBY, "1")
    a = 1
    puts a
  RUBY

  assert_equal(<<~RUBY, '"hello"')
    a = b = 'hello'
    p b
  RUBY

  assert_equal(<<~RUBY, '"hello"')
    a = b = 'hello'
    p a
  RUBY

  assert_equal(<<~RUBY, "4\n2\n2\n2")
    a = 1
    1.times do
      a += 3
      p a
      b = a = 2
      p a, b
    end
    p a
  RUBY
end
