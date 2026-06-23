class UARTTest < Picotest::Test
  def setup
    @uart = UART.new(unit: :PICORB_UART_RP2040_UART0, baudrate: 115200)
  end

  def test_initialize
    assert_equal 0, @uart.instance_variable_get(:@unit_num)
    assert_equal 115200, @uart.baudrate
  end

  def test_puts
    assert_equal nil, @uart.puts("Hello, UART!")
  end

  def test_getbyte_returns_nil_when_rx_buffer_is_empty
    assert_nil @uart.getbyte
  end

  def test_ungetbyte
    assert_nil @uart.ungetbyte(0x141)
    assert_equal 0x41, @uart.getbyte
    assert_nil @uart.getbyte
  end
end
