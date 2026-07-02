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

class WebAudioTestContext
  attr_reader :oscillators, :gains, :panners

  def initialize
    @current_time = 1.0
    @destination = WebAudioTestNode.new
    @oscillators = []
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
    @synth = WebAudio::Synth.new(context: @context, voice_count: 2)
  end

  def test_note_on_builds_oscillator_and_voice_state
    assert @synth.handle_midi([:note_on, 0, 69, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    assert_equal "sine", oscillator.properties[:type]
    assert_equal 440.0, oscillator[:frequency].value
    assert_equal :active, @synth.active_voices[0][:status]
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
    synth = WebAudio::Synth.new(context: @context, voice_count: 1)
    synth.handle_midi([:note_on, 0, 60, 100], :serial, 0, 10)
    old = @context.oscillators[-1]
    synth.handle_midi([:note_on, 0, 64, 100], :serial, 0, 20)
    old.finish
    assert_equal 64, synth.active_voices[0][:note]
  end

  def test_pitch_bend_and_program_update_active_voice
    @synth.handle_midi([:note_on, 0, 69, 100], :serial, 0, 10)
    oscillator = @context.oscillators[-1]
    @synth.handle_midi([:pitch_bend, 0, 16_383], :serial, 0, 20)
    assert_equal 200.0, oscillator[:detune].value
    @synth.handle_midi([:program_change, 0, 3], :serial, 0, 30)
    assert_equal "sawtooth", oscillator.properties[:type]
  end

  def test_pan_matches_the_demo_control_direction
    @synth.handle_midi([:control_change, 0, 10, 127], :serial, 0, 10)
    assert_equal(-1.0, @context.panners[-1][:pan].value)
    @synth.handle_midi([:control_change, 0, 10, 0], :serial, 0, 20)
    assert_equal 1.0, @context.panners[-1][:pan].value
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
    @input = WebAudio::WebSerialMIDIInput.new(router: @router)
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
