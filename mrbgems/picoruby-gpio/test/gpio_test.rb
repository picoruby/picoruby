class GpioTest < Picotest::Test
  def setup
  end

  def test_initialize_with_out
    gpio = GPIO.new(1, GPIO::OUT)
    assert_equal(GPIO, gpio.class)
  end

  def test_initialize_with_in_pull_up
    gpio = GPIO.new(2, GPIO::IN | GPIO::PULL_UP)
    assert_equal(GPIO::PULL_UP, gpio.instance_variable_get(:@pull))
  end

  def test_initialize_with_exclusive_flags
    assert_raise(ArgumentError) do
      GPIO.new(3, GPIO::IN | GPIO::OUT)
    end
    assert_raise(ArgumentError) do
      GPIO.new(4, GPIO::PULL_UP | GPIO::PULL_DOWN)
    end
  end

  def test_setmode
    gpio = GPIO.new(5, GPIO::IN)
    assert_equal(GPIO::IN, gpio.instance_variable_get(:@dir))
    gpio.setmode(GPIO::OUT)
    assert_equal(GPIO::OUT, gpio.instance_variable_get(:@dir))
  end

end
