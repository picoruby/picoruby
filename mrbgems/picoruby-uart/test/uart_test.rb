class UARTTest < Picotest::Test
  def setup
    UART.mock_reset
  end

  def test_initialize
    uart = UART.new(unit: "UART1", baudrate: 115200)
    assert_equal "UART1", UART.mock_get_unit
    assert_equal 115200, UART.mock_get_baudrate
    assert_equal 8, UART.mock_get_data_bits
    assert_equal 1, UART.mock_get_stop_bits
    assert_equal UART::PARITY_NONE, UART.mock_get_parity
  end

  def test_puts
    uart = UART.new(unit: "UART1")
    uart.puts "hello"
    assert_equal "hello\n", UART.mock_get_written_data
  end
end