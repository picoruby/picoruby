# Mock for ADC C extension
class ADC
  # Using global variables as a substitute for class variables
  $mock_adc_pin ||= nil
  $mock_adc_value ||= 0

  def _init(pin)
    $mock_adc_pin = pin
  end

  def read_u16
    $mock_adc_value
  end

  # Helper methods for testing
  def self.mock_get_pin
    $mock_adc_pin
  end

  def self.mock_set_value(value)
    $mock_adc_value = value
  end

  def self.mock_reset
    $mock_adc_pin = nil
    $mock_adc_value = 0
  end
end
