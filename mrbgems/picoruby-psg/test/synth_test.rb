class PSGTuningTest < Picotest::Test
  def teardown
    PSG.set_tuning
  end

  def test_note_to_period_supports_round_keyword
    PSG.set_tuning
    assert_equal 239, PSG.note_to_period(60)
    assert_equal 238, PSG.note_to_period(60, round: false)
  end

  def test_set_tuning_supports_pitch_keyword
    PSG.set_tuning(pitch: 880)
    assert_equal 71, PSG.note_to_period(69)
    PSG.set_tuning(:equal, pitch: 440)
    assert_equal 142, PSG.note_to_period(69)
  end

  def test_just_tuning_updates_period_table
    PSG.set_tuning
    equal_e = PSG.note_to_period(64)
    PSG.set_tuning(:just_c_major)
    assert_not_equal equal_e, PSG.note_to_period(64)
  end

  def test_enharmonic_just_tunings_share_table
    PSG.set_tuning(:just_e_flat_minor)
    e_flat = PSG.note_to_period(63)
    PSG.set_tuning(:just_d_sharp_minor)
    assert_equal e_flat, PSG.note_to_period(63)
  end
end

class PSGSynthFakeDriver
  attr_reader :calls

  def initialize
    @calls = []
  end

  def send_reg(*args); @calls << [:send_reg, *args]; true; end
  def mute(*args); @calls << [:mute, *args]; true; end
  def set_pan(*args); @calls << [:set_pan, *args]; true; end
  def set_timbre(*args); @calls << [:set_timbre, *args]; true; end
  def set_legato(*args); @calls << [:set_legato, *args]; true; end
  def set_lfo(*args); @calls << [:set_lfo, *args]; true; end
  def drum(*args); @calls << [:drum, *args]; true; end
  def mute_direct(*args); @calls << [:mute_direct, *args]; nil; end
  def buffer_flush; nil; end
end

class PSGSynthTest < Picotest::Test
  def setup
    @driver = PSGSynthFakeDriver.new
    @synth = PSG::Synth.new(@driver).start
  end

  def teardown
    @synth.stop.join
  end

  def process(event, source = :mml, priority = 0)
    @synth.handle(event, source: source, priority: priority)
    Task.pass
  end

  def test_note_ownership_includes_source
    process([:note_on, 0, 60, 100], :mml, 0)
    process([:note_on, 0, 60, 100], :uart, 100)
    assert_not_nil @synth.allocator.voice_for(0, 60, source: :mml)
    assert_not_nil @synth.allocator.voice_for(0, 60, source: :uart)
    process([:note_off, 0, 60, 0], :mml, 0)
    assert_nil @synth.allocator.voice_for(0, 60, source: :mml)
    assert_not_nil @synth.allocator.voice_for(0, 60, source: :uart)
  end

  def test_uart_priority_prevents_mml_steal
    process([:note_on, 0, 60, 100], :uart, 100)
    process([:note_on, 0, 62, 100], :uart, 100)
    process([:note_on, 0, 64, 100], :uart, 100)
    process([:note_on, 0, 65, 100], :mml, 0)
    assert_nil @synth.allocator.voice_for(0, 65, source: :mml)
  end

  def test_uart_steals_mml_voice
    process([:note_on, 0, 60, 100], :mml, 0)
    process([:note_on, 0, 62, 100], :mml, 0)
    process([:note_on, 0, 64, 100], :mml, 0)
    process([:note_on, 0, 65, 100], :uart, 100)
    assert_not_nil @synth.allocator.voice_for(0, 65, source: :uart)
    assert_nil @synth.allocator.voice_for(0, 60, source: :mml)
  end

  def test_psg_extension_is_applied
    process([:note_on, 0, 60, 100])
    process([:psg, 0, :lfo, 200, 5])
    assert @driver.calls.include?([:set_lfo, 0, 200, 5, 0])
  end

  def test_midi_channel_10_triggers_drum_with_reserved_voice
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :uart, 100)
    assert @driver.calls.include?([:drum, 38, 100, 0])
    assert_equal 0, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :uart)
  end

  def test_midi_channel_10_unknown_note_still_goes_to_drum
    process([:note_on, PSG::DRUM_CHANNEL, 60, 100], :uart, 100)
    assert @driver.calls.include?([:drum, 60, 100, 0])
  end

  def test_midi_channel_10_steals_oldest_voice_for_drum_headroom
    process([:note_on, 0, 60, 100], :uart, 100)
    process([:note_on, 0, 62, 100], :uart, 100)
    process([:note_on, 0, 64, 100], :uart, 100)
    assert_equal [:uart, 0, 60, 100, 1], @synth.allocator.entry(0)
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :uart, 100)
    assert @driver.calls.include?([:mute, 0, 1, 0])
    assert @driver.calls.include?([:drum, 38, 100, 0])
    assert_equal 0, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :uart)
    assert_nil @synth.allocator.voice_for(0, 60, source: :uart)
  end
end
