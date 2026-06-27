class MIDIBASERouterSink
  attr_reader :received
  def initialize
    @received = []
  end
  def handle(event, **context)
    @received << [event, context]
  end
end

class MIDIBASETest < Picotest::Test
  def events_for(bytes, max_sysex_bytes: 1024)
    parser = MIDIBASE::Parser.new(max_sysex_bytes: max_sysex_bytes)
    events = []
    bytes.each do |byte|
      event = parser.feed(byte)
      events << event if event
    end
    events
  end

  def test_channel_voice_and_running_status
    events = events_for([0x90, 60, 100, 61, 0, 0xE2, 0, 64])
    assert_equal [:note_on, 0, 60, 100], events[0]
    assert_equal [:note_off, 0, 61, 0], events[1]
    assert_equal [:pitch_bend, 2, 8192], events[2]
  end

  def test_all_channel_voice_messages
    events = events_for([
      0x80, 60, 10,
      0xA1, 61, 11,
      0xB2, 7, 100,
      0xC3, 12,
      0xD4, 13
    ])
    assert_equal [:note_off, 0, 60, 10], events[0]
    assert_equal [:polyphonic_key_pressure, 1, 61, 11], events[1]
    assert_equal [:control_change, 2, 7, 100], events[2]
    assert_equal [:program_change, 3, 12], events[3]
    assert_equal [:channel_pressure, 4, 13], events[4]
  end

  def test_all_realtime_messages
    events = events_for([0xF8, 0xFA, 0xFB, 0xFC, 0xFE, 0xFF])
    assert_equal [:timing_clock], events[0]
    assert_equal [:start], events[1]
    assert_equal [:continue], events[2]
    assert_equal [:stop], events[3]
    assert_equal [:active_sensing], events[4]
    assert_equal [:system_reset], events[5]
  end

  def test_realtime_does_not_interrupt_running_message
    events = events_for([0x90, 60, 0xF8, 100])
    assert_equal [:timing_clock], events[0]
    assert_equal [:note_on, 0, 60, 100], events[1]
  end

  def test_system_common_messages
    events = events_for([0xF1, 0x12, 0xF2, 1, 2, 0xF3, 7, 0xF6])
    assert_equal [:time_code_quarter_frame, 0x12], events[0]
    assert_equal [:song_position, 257], events[1]
    assert_equal [:song_select, 7], events[2]
    assert_equal [:tune_request], events[3]
  end

  def test_sysex_and_overflow_recovery
    events = events_for([0xF0, 1, 2, 0xF7, 0xF0, 1, 2, 3, 0xF7, 0x90, 64, 1], max_sysex_bytes: 2)
    assert_equal [:system_exclusive, "\x01\x02"], events[0]
    assert_equal [:system_exclusive_error, :too_large], events[1]
    assert_equal [:note_on, 0, 64, 1], events[2]
  end

  def test_encoder
    assert_equal [0x91, 60, 100], MIDIBASE.encode(:note_on, 1, 60, 100)
    assert_equal [0xE0, 0, 64], MIDIBASE.encode(:pitch_bend, 0, 8192)
    assert_equal [0xF0, 1, 2, 0xF7], MIDIBASE.encode(:system_exclusive, "\x01\x02")
    assert_raise(ArgumentError) { MIDIBASE.encode(:note_on, 16, 60, 100) }
    assert_raise(ArgumentError) { MIDIBASE.encode(:system_exclusive, "\x80") }
  end

  def test_common_transport_api
    skip "dynamic transport test requires Class" if femtoruby?
    transport_class = Class.new do
      include ::MIDIBASE

      attr_reader :written

      def initialize(bytes)
        @bytes = bytes
        @written = []
        initialize_midibase
      end

      private def midi_read_byte
        @bytes.shift
      end

      private def midi_write_byte(byte)
        @written << byte
      end
    end
    transport = transport_class.new([0x90, 60, 100])
    assert_equal [:note_on, 0, 60, 100], transport.getevent
    assert_equal 3, transport.putevent(:note_off, 0, 60, 20)
    assert_equal [0x80, 60, 20], transport.written
  end

  def test_clock_bpm_and_transport_position
    clock = MIDIBASE::Clock.new
    clock.observe([:start], 0)
    i = 0
    while i <= 24
      clock.observe([:timing_clock], i * 1000)
      i += 1
    end
    assert_in_delta 2500.0, clock.bpm, 0.001
    assert_equal [1, 2, 1], clock.position
    clock.observe([:stop], 25_000)
    clock.observe([:timing_clock], 25_000)
    assert_equal [1, 2, 1], clock.position
    clock.observe([:continue], 26_000)
    clock.observe([:timing_clock], 26_000)
    assert_equal [1, 2, 2], clock.position
  end

  def test_twelve_eight_position
    clock = MIDIBASE::Clock.new(time_signature: [12, 8])
    clock.observe([:start], 0)
    12.times { |i| clock.observe([:timing_clock], i * 1000) }
    assert_equal [1, 2, 0], clock.position
    132.times { |i| clock.observe([:timing_clock], (i + 12) * 1000) }
    assert_equal [2, 1, 0], clock.position
  end

  def test_transport_restart_resets_tempo_measurement_window
    clock = MIDIBASE::Clock.new
    clock.observe([:timing_clock], 0)
    clock.observe([:start], 1_000_000)
    i = 0
    while i <= 24
      clock.observe([:timing_clock], 1_000_000 + i * 1000)
      i += 1
    end
    assert_in_delta 2500.0, clock.bpm, 0.001
  end

  def test_voice_allocator_steals_oldest_voice
    allocator = MIDIBASE::VoiceAllocator.new(voices: 3)
    assert_equal 0, allocator.allocate(0, 60)
    assert_equal 1, allocator.allocate(0, 64)
    assert_equal 2, allocator.allocate(0, 67)
    assert_equal 0, allocator.allocate(0, 69)
    assert_nil allocator.voice_for(0, 60)
    assert_equal 1, allocator.release(0, 64)
    assert_nil allocator.release(0, 64)
  end

  def test_voice_allocator_respects_source_priority
    allocator = MIDIBASE::VoiceAllocator.new(voices: 2)
    assert_equal 0, allocator.allocate(0, 60, source: :mml, priority: 0)
    assert_equal 1, allocator.allocate(0, 62, source: :uart, priority: 100)
    assert_equal 0, allocator.allocate(0, 64, source: :mml, priority: 0)
    assert_nil allocator.allocate(0, 65, source: :mml, priority: -1)
    assert_equal 1, allocator.voice_for(0, 62, source: :uart)
    assert_nil allocator.voice_for(0, 62, source: :mml)
  end

  def test_voice_allocator_reserves_specific_voice
    allocator = MIDIBASE::VoiceAllocator.new(voices: 3)
    assert_equal 0, allocator.allocate(0, 60, source: :mml, priority: 0)
    assert_equal 1, allocator.allocate(0, 64, source: :mml, priority: 0)
    assert_equal 2, allocator.allocate(0, 67, source: :mml, priority: 0)
    assert_equal 2, allocator.reserve(2, 9, 38, source: :uart, priority: 100)
    assert_equal [:mml, 0, 67, 0, 3], allocator.last_stolen
    assert_equal 2, allocator.voice_for(9, 38, source: :uart)
    assert_nil allocator.voice_for(0, 67, source: :mml)
  end

  def test_voice_allocator_limits_candidates
    allocator = MIDIBASE::VoiceAllocator.new(voices: 3)
    assert_equal 0, allocator.allocate(0, 48, source: :mml, voice_ids: [0])
    assert_equal 1, allocator.allocate(0, 60, source: :uart, voice_ids: [1, 2])
    assert_equal 2, allocator.allocate(0, 64, source: :uart, voice_ids: [1, 2])
    assert_equal 1, allocator.allocate(0, 67, source: :uart, voice_ids: [1, 2])
    assert_not_nil allocator.voice_for(0, 48, source: :mml)
    assert_nil allocator.voice_for(0, 60, source: :uart)
  end

  def test_router_filters_and_passes_context
    sink = MIDIBASERouterSink.new
    router = MIDIBASE::Router.new
    router.connect(:uart, sink, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
    router.emit(:uart, [:timing_clock], timestamp_us: 10)
    router.emit(:uart, [:note_on, 0, 60, 100], timestamp_us: 20)
    assert_equal 1, sink.received.size
    assert_equal :uart, sink.received[0][1][:source]
    assert_equal 100, sink.received[0][1][:priority]
    assert_equal 20, sink.received[0][1][:timestamp_us]
  end
end
