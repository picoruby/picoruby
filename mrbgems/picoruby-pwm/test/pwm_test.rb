class PWMTest < Picotest::Test
  def setup
  end

  def test_initialize
    pwm = PWM.new(26, frequency: 1000, duty: 50)
    assert_equal 26, pwm.instance_variable_get(:@pin)
    assert_equal 1000, pwm.instance_variable_get(:@frequency)
    assert_equal 50, pwm.instance_variable_get(:@duty)
  end

end
