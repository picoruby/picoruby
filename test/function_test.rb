class FunctionTest < MmrubyTest
  desc "One arg without paren"
  assert_equal(<<~RUBY, "Hello")
    puts 'Hello'
  RUBY

  desc "One arg with paren"
  assert_equal(<<~RUBY, "Hello")
    puts('Hello')
  RUBY

  desc "Multiple args without paren"
  assert_equal(<<~RUBY, "String\nsymbol\n1\n3.14")
    puts 'String', :symbol, 1, 3.14
  RUBY

  desc "Multiple args with paren"
  assert_equal(<<~RUBY, "String\nsymbol\n1\n3.14")
    puts('String', :symbol, 1, 3.14)
  RUBY
end
