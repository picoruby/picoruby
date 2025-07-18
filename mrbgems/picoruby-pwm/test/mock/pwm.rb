# Mock for PWM C extension
class PWM
  # Using global variables as a substitute for class variables
  $mock_pwm_pin ||= nil
  $mock_pwm_freq ||= 0
  $mock_pwm_duty ||= 0

  def _init(pin)
    $mock_pwm_pin = pin
  end

  def frequency(freq)
    $mock_pwm_freq = freq
  end

  def duty_u16(duty)
    $mock_pwm_duty = duty
  end

  # Helper methods for testing
  def self.mock_get_pin
    $mock_pwm_pin
  end

  def self.mock_get_freq
    $mock_pwm_freq
  end

  def self.mock_get_duty
    $mock_pwm_duty
  end

  def self.mock_reset
    $mock_pwm_pin = nil
    $mock_pwm_freq = 0
    $mock_pwm_duty = 0
  end
end
