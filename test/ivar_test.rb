class IvarTest < PicoRubyTest
  desc "assign"
  assert_equal(<<~RUBY, '[0]')
    @ivar = [0]
    p @ivar
  RUBY

  desc "init idiom"
  assert_equal(<<~RUBY, '"init"')
    @ivar ||= 'init'
    p @ivar
  RUBY

  desc "op_assign"
  assert_equal(<<~RUBY, '2')
    @ivar = 1
    @ivar += 1
    p @ivar
  RUBY
end
