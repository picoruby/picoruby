class ADCTest < Picotest::Test
  def setup
    ADC.mock_reset
  end

  def test_initialize
    adc = ADC.new(26)
    assert_equal 26, ADC.mock_get_pin
  end

  def test_read_u16
    ADC.mock_set_value(12345)
    adc = ADC.new(27)
    assert_equal 12345, adc.read_u16
  end
end
