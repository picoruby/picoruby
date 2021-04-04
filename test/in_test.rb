class IfTest < PicoRubyTest
  pending

  desc "use if value 1"
  assert_equal(<<~RUBY, '1')
    res = if true
      1
    else
      0
    end
    p res
  RUBY

  desc "use if value 2"
  assert_equal(<<~RUBY, '0')
    res = if false
      1
    else
      0
    end
    p res
  RUBY
end

