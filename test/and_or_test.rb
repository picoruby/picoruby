class AndOrTest < PicoRubyTest
  desc "and"
  assert_equal(<<~RUBY, "nil")
    p true && nil
  RUBY

  desc "or 1"
  assert_equal(<<~RUBY, "true")
    puts nil || true
  RUBY

  desc "or 2"
  assert_equal(<<~RUBY, "true")
    puts true || false
  RUBY
end
