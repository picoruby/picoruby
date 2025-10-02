class ADCTest < Picotest::Test
  def setup
  end

  def test_initialize
    adc = ADC.new(26)
    assert_equal 26, adc.input
  end

  def test_read_raw
    adc = ADC.new(27)
    stub(adc).read_raw { 12345 }
    assert_equal 12345, adc.read_raw
  end
end
