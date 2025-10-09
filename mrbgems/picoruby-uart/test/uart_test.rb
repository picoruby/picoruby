class UARTTest < Picotest::Test
  def setup
    @uart = UART.new(unit: :PICORUBY_UART_RP2040_UART0, baudrate: 115200)
  end

  def test_initialize
    assert_equal 0, @uart.instance_variable_get(:@unit_num)
    assert_equal 115200, @uart.baudrate
  end

  def test_puts
    assert_equal nil, @uart.puts("Hello, UART!")
  end
end
