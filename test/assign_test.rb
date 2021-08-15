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

  assert_equal(<<~RUBY, "1")
    class A
      attr_accessor :b
    end
    a = A.new
    a.b = 1
    puts a.b
  RUBY

  assert_equal(<<~RUBY, "[2, 1]")
    a = [0, 1]
    a[0] = 2
    p a
  RUBY
end
