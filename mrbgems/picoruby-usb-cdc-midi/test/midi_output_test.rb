class USBCDCMIDIOutputTest < Picotest::Test
  def test_encodes_channel_event_as_binary_string
    output = build_output
    assert_equal 3, output.putevent(:note_on, 1, 60, 100)
    assert_equal [0x91, 60, 100], bytes_of(output.writes[0])
  end

  def test_works_as_router_sink
    output = build_output
    router = MIDIBASE::Router.new
    router.connect(:uart, output, only: MIDIBASE::WIRE_EVENTS)
    router.emit(:uart, [:pitch_bend, 2, 8192], timestamp_us: 100)
    assert_equal [0xE2, 0, 64], bytes_of(output.writes[0])
  end

  def test_returns_false_when_disconnected
    output = build_output(connected: false)
    assert_equal false, output.putevent(:note_on, 0, 60, 100)
    assert_equal [], output.writes
  end

  def test_sys_ex_can_be_written_in_chunks
    output = build_output(chunk_size: 3)
    data = "\x01\x02\x03\x04"
    assert_equal 6, output.putevent(:system_exclusive, data)
    assert_equal [0xF0, 1, 2], bytes_of(output.writes[0])
    assert_equal [3, 4, 0xF7], bytes_of(output.writes[1])
  end

  private def bytes_of(string)
    bytes = []
    i = 0
    bytesize = string.bytesize
    while i < bytesize
      bytes << string.getbyte(i)
      i += 1
    end
    bytes
  end

  private def build_output(connected: true, chunk_size: nil)
    klass = Class.new(USB::CDC::MIDIOutput) do
      attr_reader :writes

      def initialize(connected:, chunk_size:)
        super()
        @connected = connected
        @chunk_size = chunk_size
        @writes = []
      end

      def connected?
        @connected
      end

      private def write_bytes(bytes)
        return false unless connected?
        chunk_size = @chunk_size
        if chunk_size
          offset = 0
          bytesize = bytes.bytesize
          while offset < bytesize
            length = chunk_size
            length = bytesize - offset if bytesize - offset < length
            @writes << bytes.byteslice(offset, length)
            offset += length
          end
        else
          @writes << bytes
        end
        true
      end
    end
    klass.new(connected: connected, chunk_size: chunk_size)
  end
end
