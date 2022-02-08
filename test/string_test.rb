class YieldTest < PicoRubyTest
  desc "Keyword in str"
  assert_equal(<<~RUBY, "nil")
    puts "nil"
  RUBY
end
