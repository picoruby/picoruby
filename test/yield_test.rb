class YieldTest < PicoRubyTest
  desc "do block"
  assert_equal(<<~'RUBY', "Hello PicoRuby")
    def my_method(m, n)
      yield m, n
    end
    my_method('Ruby', 'Pico') do |v, x|
      puts \"Hello #{x}#{v}\"
    end
  RUBY
end
