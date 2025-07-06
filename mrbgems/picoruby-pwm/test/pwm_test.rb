class PWMTest < Picotest::Test
  def setup
    PWM.mock_reset
  end

  def test_initialize
    pwm = PWM.new(26, frequency: 1000, duty: 50)
    assert_equal 26, PWM.mock_get_pin
    assert_equal 1000, PWM.mock_get_freq
  end

  def test_duty_u16
    pwm = PWM.new(27)
    pwm.duty_u16(32767)
    assert_equal 32767, PWM.mock_get_duty
  end
end
