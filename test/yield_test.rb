class YieldTest < PicoRubyTest
  desc "do block 1"
  assert_equal(<<~'RUBY', "Hello Ruby")
    def my_method(m)
      yield m
    end
    my_method('Ruby') do |v|
      puts \"Hello #{v}\"
    end
  RUBY

  desc "do block 2"
  assert_equal(<<~'RUBY', "Hello PicoRuby")
    def my_method(m, n)
     yield m, n
    end
    my_method('Ruby', 'Pico') do |v, x|
      puts \"Hello #{x}#{v}\"
    end
  RUBY
end
