class UARTTest < Picotest::Test
  def setup
    @uart = UART.new(unit: :RP2040_UART0, baudrate: 115200)
  end

  def test_initialize
    assert_equal 1, @uart.instance_variable_get(:@unit_num)
    assert_equal 115200, @uart.baudrate
  end

  def test_puts
    assert_equal nil, @uart.puts("Hello, UART!")
  end

  def test_bitbang_initialize
    uart = UART.new(unit: :BITBANG, pin: 4, baudrate: 9600)
    assert_equal 0, uart.instance_variable_get(:@unit_num)
    assert_equal true, uart.instance_variable_get(:@bitbang)
    assert_equal 9600, uart.baudrate
  end
end
