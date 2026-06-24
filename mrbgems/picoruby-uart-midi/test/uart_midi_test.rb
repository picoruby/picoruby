class UARTMIDITest < Picotest::Test
  def setup
    @midi = UART::MIDI.new(unit: :PICORB_UART_RP2040_UART0)
  end

  def test_initializes_uart_for_midi
    uart = @midi.instance_variable_get(:@uart)
    assert_equal 31_250, uart.baudrate
  end

  def test_getevent_from_uart_buffer
    uart = @midi.instance_variable_get(:@uart)
    uart.ungetbyte(100)
    uart.ungetbyte(60)
    uart.ungetbyte(0x90)
    assert_equal [:note_on, 0, 60, 100], @midi.getevent
  end
end
