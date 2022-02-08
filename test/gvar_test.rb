class GvarTest < PicoRubyTest
  desc "assign"
  assert_equal(<<~RUBY, '[0]')
    $gvar = [0]
    p $gvar
  RUBY

  desc "init idiom"
  assert_equal(<<~RUBY, '"init"')
    $gvar ||= "init"
    p $gvar
  RUBY

  desc "op_assign"
  assert_equal(<<~RUBY, '2')
    $gvar = 1
    $gvar += 1
    p $gvar
  RUBY

  desc "op_assign array"
  assert_equal(<<~RUBY, '[]')
    $gvar||=[]
    p $gvar
  RUBY

  desc "gvar op_assign hash"
  assert_equal(<<~RUBY, '{}')
    $gvar||={}
    p $gvar
  RUBY
end
