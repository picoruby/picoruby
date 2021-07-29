class YieldTest < PicoRubyTest
  desc "do block"
  assert_equal(<<~'RUBY', "Hello Ruby")
    def my_method(v)
      yield v
    end
    my_method('Ruby') do |v|
      puts \"Hello #{v}\"
    end
  RUBY
end
