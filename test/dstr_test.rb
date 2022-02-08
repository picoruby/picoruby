class DstrTest < PicoRubyTest
  desc "interpolation"
  assert_equal(<<~'RUBY', 'hello Ruby')
    @ivar = "Ruby"
    puts "hello #{@ivar}"
  RUBY
end
