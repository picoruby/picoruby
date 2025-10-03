class QuectelCellularTest < Picotest::Test
  def setup
    @mock_uart = UART.new(unit: :PICORUBY_UART_RP2040_UART0, baudrate: 115200)
    @client = QuectelCellular.new(uart: @mock_uart, log_size: 5)
  end

  def teardown
    clear_doubles
  end

  def test_initialize
    assert_equal 5, @client.instance_variable_get(:@log_size)
    assert_equal [], @client.log
    assert_equal "SORACOM.IO", @client.apn
    assert_equal "sora", @client.username
    assert_equal "sora", @client.password
    assert_equal "picoruby", @client.user_agent
    assert_equal "\r", @mock_uart.instance_variable_get(:@line_ending)
  end

  def test_initialize_default_log_size
    mock_uart = UART.new(unit: :PICORUBY_UART_RP2040_UART0, baudrate: 115200)
    client = QuectelCellular.new(uart: mock_uart)
    assert_equal 10, client.instance_variable_get(:@log_size)
  end

  def test_simple_uart_mock
    stub(@mock_uart).puts { nil }
    @mock_uart.puts("test")
    # If we get here, the stub worked
    assert_true true
  end

#  def test_command_with_immediate_response
#    # Mock UART to simulate immediate OK response
#    stub(@mock_uart).puts { nil }
#    call_count = 0
#    stub(@mock_uart).gets do
#      call_count += 1
#      if call_count == 1
#        "OK\r\n"
#      else
#        nil
#      end
#    end
#
#    result = @client.command("AT", "OK")
#
#    assert_true result
#    assert_equal 1, @client.log.length
#    assert_equal "AT", @client.log.first[:cmd]
#    assert @client.log.first[:res].include?("OK")
#  end
end
