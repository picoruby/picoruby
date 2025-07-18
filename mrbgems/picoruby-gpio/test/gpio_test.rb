class GpioTest < Picotest::Test
  def setup
    # The mock file defines constants like IN, OUT, etc.
    # and also the mock methods for the C extension.
    # So we need to load it before running the tests.
    # The test runner will load the mock file automatically.
    GPIO.mock_reset
  end

  def test_initialize_with_out
    gpio = GPIO.new(1, GPIO::OUT)
    assert_equal(GPIO::OUT, GPIO.mock_get_dir(1))
  end

  def test_initialize_with_in_pull_up
    gpio = GPIO.new(2, GPIO::IN | GPIO::PULL_UP)
    assert_equal(GPIO::IN, GPIO.mock_get_dir(2))
    assert_equal(:up, GPIO.mock_get_pull(2))
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
    gpio.setmode(GPIO::OUT | GPIO::PULL_DOWN)
    assert_equal(GPIO::OUT, GPIO.mock_get_dir(5))
    assert_equal(:down, GPIO.mock_get_pull(5))
  end

  def test_alt_function
    gpio = GPIO.new(6, GPIO::ALT, 7)
    assert_equal(7, GPIO.mock_get_function(6))
  end
end
