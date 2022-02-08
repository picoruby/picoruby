class CaseTest < PicoRubyTest
  desc "case 1"
  assert_equal(<<~RUBY, "true")
    dummy = [0,1,2]
    res = case dummy[1]
    when 1
      dummy[0]
      true
    when 2
      false
    end
    p res
  RUBY

  desc "case 2"
  assert_equal(<<~RUBY, "false")
    dummy = [0,1,2]
    res = case dummy[2]
    when 1
      true
    when 2
      dummy[0]
      false
    end
    p res
  RUBY

  desc "case no else"
  assert_equal(<<~RUBY, "nil")
    dummy = [0,1,2]
    res = case dummy[0]
    when 1
      true
    when 2
      false
    end
    p res
  RUBY

  desc "case else"
  assert_equal(<<~RUBY, ":ruby")
    dummy = 1
    res = case dummy[0]
    when 2
      false
    else
      String.new
      [{a: 2}, 'hello', :ruby][2]
    end
    p res
  RUBY

end
