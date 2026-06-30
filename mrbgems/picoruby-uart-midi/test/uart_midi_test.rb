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
    assert @midi.last_event_timestamp_us.is_a?(Integer)
  end

  def test_handle_writes_event_bytes_for_router_sink
    capture_class = Class.new(UART::MIDI) do
      attr_reader :written

      def initialize(unit:)
        @written = []
        super(unit: unit)
      end

      private def midi_write_byte(byte)
        @written << byte
        byte
      end
    end
    midi = capture_class.new(unit: :PICORB_UART_RP2040_UART0)
    router = MIDIBASE::Router.new
    router.connect(:mml, midi, only: MIDIBASE::WIRE_EVENTS)
    router.emit(:mml, [:note_on, 1, 60, 100], timestamp_us: 1)
    assert_equal [0x91, 60, 100], midi.written
  end
end
