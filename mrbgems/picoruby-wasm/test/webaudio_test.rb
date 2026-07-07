class WebAudioTestParam
  attr_reader :value, :calls

  def initialize(value = 0.0)
    @value = value
    @calls = []
  end

  def [](name)
    name == :value ? @value : nil
  end

  def setValueAtTime(value, time)
    @value = value
    @calls << [:set, value, time]
  end

  def linearRampToValueAtTime(value, time)
    @value = value
    @calls << [:ramp, value, time]
  end

  def exponentialRampToValueAtTime(value, time)
    @value = value
    @calls << [:exponential_ramp, value, time]
  end

  def cancelScheduledValues(time)
    @calls << [:cancel, time]
  end
end

class WebAudioTestNode
  attr_reader :properties, :connections

  def initialize
    @properties = {}
    @connections = []
    @params = {}
  end

  def [](name)
    return @properties[name] if @properties.key?(name)
    @params[name] ||= WebAudioTestParam.new
  end

  def []=(name, value)
    @properties[name] = value
  end

  def connect(node)
    @connections << node
    node
  end

  def disconnect
    @connections = []
  end
end

class WebAudioTestOscillator < WebAudioTestNode
  attr_reader :started_at, :stopped_at

  def addEventListener(name, &block)
    @ended = block if name == "ended"
    1
  end

  def start(time)
    @started_at = time
  end

  def stop(time)
    @stopped_at = time
  end

  def finish
    @ended.call(nil) if @ended
  end
end

class WebAudioTestBuffer < WebAudioTestNode
  attr_reader :samples

  def initialize(length)
    super()
    @samples = Array.new(length, 0.0)
  end

  def getChannelData(_channel)
    @samples
  end
end

class WebAudioTestContext
  attr_reader :oscillators, :buffer_sources, :gains, :panners

  def initialize
    @current_time = 1.0
    @destination = WebAudioTestNode.new
    @oscillators = []
    @buffer_sources = []
    @gains = []
    @panners = []
  end

  def [](name)
    case name
    when :currentTime then @current_time
    when :sampleRate then 48_000
    when :destination then @destination
    when :state then "running"
    end
  end

  def createGain
    node = WebAudioTestNode.new
    @gains << node
    node
  end

  def createStereoPanner
    node = WebAudioTestNode.new
    @panners << node
    node
  end

  def createBiquadFilter
    WebAudioTestNode.new
  end

  def createAnalyser
    WebAudioTestNode.new
  end

  def createOscillator
    oscillator = WebAudioTestOscillator.new
    @oscillators << oscillator
    oscillator
  end

  def createBuffer(_channels, length, _sample_rate)
    WebAudioTestBuffer.new(length)
  end

  def createBufferSource
    source = WebAudioTestOscillator.new
    @buffer_sources << source
    source
  end

  def resume
    true
  end

  def close
    true
  end
end

class WebAudioSynthTest < Picotest::Test
  def setup
    @context = WebAudioTestContext.new
    @synth = JS::WebAudio::Synth.new(context: @context, voice_count: 2)
  end

  def test_note_on_builds_oscillator_and_voice_state
    assert @synth.handle_midi([:note_on, 0, 69, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    assert_equal "sine", oscillator.properties[:type]
    assert_equal 440.0, oscillator[:frequency].value
    assert_equal :active, @synth.active_voices[0][:status]
  end

  def test_note_on_schedules_a_short_attack_after_current_time
    assert @synth.handle_midi([:note_on, 0, 69, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    gain = @context.gains[1][:gain]
    start_time = 1.0 + JS::WebAudio::NOTE_START_DELAY
    assert_equal start_time, oscillator.started_at
    assert_equal [:set, 0.0, start_time], gain.calls[1]
    assert_equal [:ramp, 100 / 127.0, start_time + JS::WebAudio::DEFAULT_TONE[:attack]], gain.calls[2]
  end

  def test_note_off_releases_then_ended_callback_frees_voice
    @synth.handle_midi([:note_on, 0, 60, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    assert @synth.handle_midi([:note_off, 0, 60, 0], :serial, 0, 20)
    assert_equal :releasing, @synth.active_voices[0][:status]
    oscillator.finish
    assert_equal [], @synth.active_voices
  end

  def test_old_ended_callback_does_not_release_reused_slot
    synth = JS::WebAudio::Synth.new(context: @context, voice_count: 1)
    synth.handle_midi([:note_on, 0, 60, 100], :serial, 0, 10)
    old = @context.oscillators[-1]
    synth.handle_midi([:note_on, 0, 64, 100], :serial, 0, 20)
    old.finish
    assert_equal 64, synth.active_voices[0][:note]
  end

  def test_pitch_bend_updates_active_voice
    @synth.handle_midi([:note_on, 0, 69, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    @synth.handle_midi([:pitch_bend, 0, 16_383], :serial, 0, 20)
    assert_equal 200.0, oscillator[:detune].value
  end

  def test_pan_matches_the_demo_control_direction
    @synth.handle_midi([:control_change, 0, 10, 127], :serial, 0, 10)
    assert_equal 1.0, @context.panners[-1][:pan].value
    @synth.handle_midi([:control_change, 0, 10, 0], :serial, 0, 20)
    assert_equal(-1.0, @context.panners[-1][:pan].value)
    @synth.handle_midi([:control_change, 0, 10, 64], :serial, 0, 30)
    assert_equal 0.0, @context.panners[-1][:pan].value
  end

  def test_all_sound_off_is_immediate
    @synth.handle_midi([:note_on, 0, 60, 100], :serial, 0, 10)
    @synth.handle_midi([:control_change, 0, 120, 0], :serial, 0, 20)
    assert_equal [], @synth.active_voices
  end

  def test_rejects_invalid_tone_value
    error = nil
    begin
      @synth.update_tone(source: :serial, channel: 0, sustain: 2.0)
    rescue => e
      error = e
    end
    assert_equal ArgumentError, error.class
  end

  def test_channel_10_has_a_fixed_percussion_tone
    before = @synth.channel_state(source: :serial, channel: 9)
    assert before[:percussion]
    @synth.update_tone(source: :serial, channel: 9, waveform: "square")
    after = @synth.channel_state(source: :serial, channel: 9)
    assert_equal before[:tone], after[:tone]
  end

  def test_channel_10_kick_uses_a_falling_sine_oscillator
    assert @synth.handle_midi([:note_on, 9, 35, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    assert_equal "sine", oscillator.properties[:type]
    assert_equal 46.0, oscillator[:frequency].value
    assert @synth.handle_midi([:note_off, 9, 35, 0], :serial, 0, 20)
    assert_equal :active, @synth.active_voices[0][:status]
  end

  def test_channel_10_snare_and_hats_use_noise
    assert @synth.handle_midi([:note_on, 9, 38, 100], :serial, 0, 10)
    assert_equal 1, @context.buffer_sources.size
    @context.buffer_sources[-1].finish

    assert @synth.handle_midi([:note_on, 9, 46, 100], :serial, 0, 20)
    open_hat = @context.buffer_sources[-1]
    assert_equal 1.5, open_hat.stopped_at
    assert @synth.handle_midi([:note_on, 9, 44, 100], :serial, 0, 30)
    assert_equal 1.075, @context.buffer_sources[-1].stopped_at
    assert_equal 1, @synth.active_voices.size
  end

  def test_channel_10_duplicate_notes_share_instruments
    notes = JS::WebAudio::PERCUSSION_NOTES
    assert_equal :kick, notes[35]
    assert_equal :kick, notes[36]
    assert_equal :snare, notes[38]
    assert_equal :snare, notes[40]
    assert_equal :closed_hat, notes[42]
    assert_equal :closed_hat, notes[44]
  end

  def test_channel_10_toms_have_distinct_falling_pitches
    expected = [[41, 78.0], [43, 78.0], [45, 110.0], [47, 110.0],
                [48, 150.0], [50, 150.0]]
    expected.each do |note, end_frequency|
      assert @synth.handle_midi([:note_on, 9, note, 100], :serial, 0, 10)
      oscillator = @context.oscillators[-1]
      assert_equal "triangle", oscillator.properties[:type]
      assert_equal end_frequency, oscillator[:frequency].value
      oscillator.finish
    end
  end

  def test_channel_10_ignores_unassigned_notes
    assert_equal false, @synth.handle_midi([:note_on, 9, 39, 100], :serial, 0, 10)
    assert_equal [], @synth.active_voices
  end
end

class WebSerialMIDIInputTest < Picotest::Test
  class Sink
    attr_reader :events
    def initialize
      @events = []
    end
    def handle(event, source: :default, priority: 0, timestamp_us: nil, **_context)
      handle_midi(event, source, priority, timestamp_us)
    end
    def handle_midi(event, source, _priority, timestamp_us)
      @events << [event, source, timestamp_us]
    end
  end

  def setup
    @router = MIDIBASE::Router.new
    @sink = Sink.new
    @router.connect(:webserial, @sink)
    @input = JS::WebAudio::WebSerialMIDIInput.new(router: @router)
  end

  def test_parses_multiple_events_and_running_status_across_chunks
    assert_equal 0, @input.feed("\x90\x3C", timestamp_us: 10)
    assert_equal 2, @input.feed("\x64\x40\x7F", timestamp_us: 20)
    assert_equal [:note_on, 0, 60, 100], @sink.events[0][0]
    assert_equal [:note_on, 0, 64, 127], @sink.events[1][0]
    assert_equal 20, @sink.events[0][2]
  end

  def test_realtime_event_does_not_break_channel_message
    @input.feed("\x90\x3C\xF8\x64", timestamp_us: 30)
    assert_equal [:timing_clock], @sink.events[0][0]
    assert_equal [:note_on, 0, 60, 100], @sink.events[1][0]
  end
end
