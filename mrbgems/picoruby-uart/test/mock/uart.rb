# Mock for UART C extension
class UART
  # Constants
  PARITY_NONE = 0
  PARITY_EVEN = 1
  PARITY_ODD  = 2
  FLOW_CONTROL_NONE = 0
  FLOW_CONTROL_RTS_CTS = 1

  # Using global variables as a substitute for class variables
  $mock_uart_unit ||= nil
  $mock_uart_baudrate ||= 0
  $mock_uart_data_bits ||= 0
  $mock_uart_stop_bits ||= 0
  $mock_uart_parity ||= 0
  $mock_uart_flow_control ||= 0
  $mock_uart_rts_pin ||= -1
  $mock_uart_cts_pin ||= -1
  $mock_uart_written_data ||= ""

  def open_rx_buffer(size)
    # no-op
  end

  def open_connection(unit, txd, rxd, buffer)
    $mock_uart_unit = unit
    0 # unit_num
  end

  def _set_baudrate(baudrate)
    $mock_uart_baudrate = baudrate
  end

  def set_flow_control(rts, cts)
    $mock_uart_flow_control = [rts, cts]
  end

  def _set_function(pin)
    # no-op
  end

  def _set_flow_control(rts, cts)
    $mock_uart_flow_control = [rts, cts]
  end

  def _set_format(data_bits, stop_bits, parity)
    $mock_uart_data_bits = data_bits
    $mock_uart_stop_bits = stop_bits
    $mock_uart_parity = parity
  end

  def write(data)
    $mock_uart_written_data << data
  end

  # Helper methods for testing
  def self.mock_get_unit
    $mock_uart_unit
  end

  def self.mock_get_baudrate
    $mock_uart_baudrate
  end

  def self.mock_get_data_bits
    $mock_uart_data_bits
  end

  def self.mock_get_stop_bits
    $mock_uart_stop_bits
  end

  def self.mock_get_parity
    $mock_uart_parity
  end

  def self.mock_get_flow_control
    $mock_uart_flow_control
  end

  def self.mock_get_written_data
    $mock_uart_written_data
  end

  def self.mock_reset
    $mock_uart_unit = nil
    $mock_uart_baudrate = 0
    $mock_uart_data_bits = 0
    $mock_uart_stop_bits = 0
    $mock_uart_parity = 0
    $mock_uart_flow_control = 0
    $mock_uart_rts_pin = -1
    $mock_uart_cts_pin = -1
    $mock_uart_written_data = ""
  end
end