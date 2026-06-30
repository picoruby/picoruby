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

class PSGVoiceProgramTest < Picotest::Test
  def teardown
    PSG.assign_drum_program(60, nil)
  end

  def test_define_voice_program_copies_and_freezes_steps
    steps = [
      [220, 3, 15, 35],
      [260, 3, 13, 45],
      [0, 4, 9, 60]
    ]
    program = PSG.define_voice_program(:test_program, steps)
    steps[0][0] = 1

    assert_equal 220, program[0][0]
    assert_equal program, PSG.voice_program(:test_program)
    assert program.frozen?
    assert program[0].frozen?
  end

  def test_arbitrary_program_can_be_assigned_to_midi_drum_note
    program = PSG.define_voice_program(:test_fire, [[120, 0, 15, 25]])
    assert_equal :test_fire, PSG.assign_drum_program(60, :test_fire)
    assert_equal program, PSG.drum_program(60)
    assert_nil PSG.assign_drum_program(60, nil)
    assert_nil PSG.drum_program(60)
  end

  def test_voice_program_rejects_invalid_data
    assert_raise(ArgumentError) { PSG.define_voice_program(:invalid, []) }
    assert_raise(ArgumentError) { PSG.define_voice_program(:invalid, [[4096, 0, 15, 1]]) }
    assert_raise(ArgumentError) { PSG.define_voice_program(:invalid, [[0, 32, 15, 1]]) }
    assert_raise(ArgumentError) { PSG.define_voice_program(:invalid, [[0, 1, 16, 1]]) }
    assert_raise(ArgumentError) { PSG.define_voice_program(:invalid, [[0, 1, 15, -1]]) }
  end

  def test_assignment_rejects_invalid_note_and_unknown_program
    assert_raise(ArgumentError) { PSG.assign_drum_program(128, :snare) }
    assert_raise(ArgumentError) { PSG.assign_drum_program(60, :missing) }
    assert_raise(ArgumentError) { PSG.voice_program(:missing) }
  end

  def test_driver_validates_voice_write
    driver = PSG::Driver.allocate
    assert_equal false, driver.voice_write(0, 120, 4, 15, 3)
    assert_raise(ArgumentError) { driver.voice_write(3, 120, 4, 15, 3) }
    assert_raise(ArgumentError) { driver.voice_write(0, 4096, 4, 15, 3) }
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
  def voice_write(*args); @calls << [:voice_write, *args]; true; end
  def mute_direct(*args); @calls << [:mute_direct, *args]; nil; end
  def buffer_flush; nil; end
end

class PSGMIDISink
  attr_reader :events

  def initialize
    @events = []
  end

  def handle(event, **context)
    @events << [event, context]
    true
  end
end

class PSGMIDILogger
  attr_reader :lines

  def initialize
    @lines = []
  end

  def puts(line)
    @lines << line
  end
end

class PSGMIDIControllerTest < Picotest::Test
  def setup
    @sink = PSGMIDISink.new
    @logger = PSGMIDILogger.new
    @controller = PSG::MIDIController.new(@sink, logger: @logger)
  end

  def handle(event, source: :uart, priority: 100, timestamp_us: 123)
    @controller.handle(event, source: source, priority: priority, timestamp_us: timestamp_us)
  end

  def test_maps_live_and_global_controls
    handle([:control_change, 0, PSG::MIDIController::CC_TIMBRE, 2])
    handle([:control_change, 0, PSG::MIDIController::CC_ENV_ENABLED, 127])
    handle([:control_change, 0, PSG::MIDIController::CC_LFO_RATE, 127])
    handle([:control_change, 0, PSG::MIDIController::CC_MIXER, 2])
    handle([:control_change, 0, PSG::MIDIController::CC_BEND_RANGE, 20])
    handle([:control_change, 0, PSG::MIDIController::CC_ENV_PERIOD, 32])
    handle([:control_change, 0, PSG::MIDIController::CC_ENV_SHAPE, 10])
    handle([:control_change, 0, PSG::MIDIController::CC_NOISE_PERIOD, 12])

    events = @sink.events
    assert_equal [:program_change, 0, 2], events[0][0]
    assert_equal [:psg, 0, :env_enabled, 1], events[1][0]
    assert_equal [:psg, 0, :lfo_rate, 255], events[2][0]
    assert_equal [:psg, 0, :mixer, 2], events[3][0]
    assert_equal [:psg, 0, :bend_range, 12], events[4][0]
    assert_equal [:psg_global, :env_period, 4096], events[5][0]
    assert_equal [:psg_global, :env_shape, 10], events[6][0]
    assert_equal [:psg_global, :noise_period, 12], events[7][0]
    assert_equal :uart, events[0][1][:source]
    assert_equal 100, events[0][1][:priority]
    assert_equal 123, events[0][1][:timestamp_us]
  end

  def test_envelope_period_covers_endpoints
    handle([:control_change, 0, PSG::MIDIController::CC_ENV_PERIOD, 0])
    handle([:control_change, 0, PSG::MIDIController::CC_ENV_PERIOD, 127])
    assert_equal [:psg_global, :env_period, 0], @sink.events[0][0]
    assert_equal [:psg_global, :env_period, 65_535], @sink.events[1][0]
  end

  def test_passes_notes_without_logging_and_deduplicates_reports
    handle([:note_on, 0, 60, 100])
    handle([:control_change, 0, 10, 64])
    handle([:control_change, 0, 10, 64])
    assert_equal [:note_on, 0, 60, 100], @sink.events[0][0]
    assert_equal 1, @logger.lines.size
    assert @logger.lines[0].include?("pan=8")
  end

  def test_validates_dependencies
    assert_raise(ArgumentError) { PSG::MIDIController.new(Object.new) }
  end
end

class PSGSynthTest < Picotest::Test
  def setup
    @driver = PSGSynthFakeDriver.new
    @synth = PSG::Synth.new(@driver).start
  end

  def teardown
    @synth.stop.join
    PSG.assign_drum_program(60, nil)
  end

  def process(event, source = :mml, priority = 0)
    @synth.handle(event, source: source, priority: priority)
    Task.pass
  end

  def use_synth(voice_pools)
    @synth.stop.join
    @driver = PSGSynthFakeDriver.new
    @synth = PSG::Synth.new(@driver, voice_pools: voice_pools).start
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

  def test_live_parameter_extensions_are_applied
    process([:note_on, 0, 60, 100], :uart)
    process([:control_change, 0, 1, 80], :uart)
    process([:psg, 0, :lfo_rate, 25], :uart)
    process([:psg, 0, :env_enabled, 1], :uart)
    process([:psg, 0, :bend_range, 12], :uart)
    assert @driver.calls.include?([:set_lfo, 0, 80, 0, 0])
    assert @driver.calls.include?([:set_lfo, 0, 80, 25, 0])
    assert @driver.calls.include?([:send_reg, 8, 16, 0])
  end

  def test_global_extensions_write_shared_registers
    process([:psg_global, :env_period, 0x1234], :uart)
    process([:psg_global, :env_shape, 10], :uart)
    process([:psg_global, :noise_period, 7], :uart)
    assert @driver.calls.include?([:send_reg, 11, 0x34, 0])
    assert @driver.calls.include?([:send_reg, 12, 0x12, 0])
    assert @driver.calls.include?([:send_reg, 13, 10, 0])
    assert @driver.calls.include?([:send_reg, 6, 7, 0])
  end

  def test_voice_pools_prevent_cross_source_stealing
    use_synth({mml: 1, uart: 2})
    process([:note_on, 0, 60, 100], :uart, 100)
    process([:note_on, 0, 64, 100], :uart, 100)
    process([:note_on, 0, 48, 100], :mml, 0)
    process([:note_on, 0, 67, 100], :uart, 100)
    assert_equal 0, @synth.allocator.voice_for(0, 48, source: :mml)
    assert_not_nil @synth.allocator.voice_for(0, 67, source: :uart)
    assert_nil @synth.allocator.voice_for(0, 60, source: :uart)
  end

  def test_live_controls_update_only_live_pool_voices
    use_synth({mml: 1, uart: 2})
    process([:note_on, 0, 48, 100], :mml)
    process([:note_on, 0, 60, 100], :uart, 100)
    process([:note_on, 0, 64, 100], :uart, 100)
    @driver.calls.clear
    process([:control_change, 0, 10, 127], :uart, 100)
    assert !@driver.calls.include?([:set_pan, 0, 15, 0])
    assert @driver.calls.include?([:set_pan, 1, 15, 0])
    assert @driver.calls.include?([:set_pan, 2, 15, 0])
  end

  def test_two_sequence_voices_leave_one_live_voice
    use_synth({mml: 2, uart: 1})
    process([:note_on, 0, 48, 100], :mml)
    process([:note_on, 1, 52, 100], :mml)
    process([:note_on, 0, 60, 100], :uart, 100)
    process([:note_on, 0, 64, 100], :uart, 100)
    assert_not_nil @synth.allocator.voice_for(0, 48, source: :mml)
    assert_not_nil @synth.allocator.voice_for(1, 52, source: :mml)
    assert_nil @synth.allocator.voice_for(0, 60, source: :uart)
    assert_equal 2, @synth.allocator.voice_for(0, 64, source: :uart)
  end

  def test_unconfigured_pool_source_cannot_sound
    use_synth({mml: 2})
    process([:note_on, 0, 60, 100], :uart)
    assert_nil @synth.allocator.voice_for(0, 60, source: :uart)
  end

  def test_voice_pool_validation
    assert_raise(ArgumentError) { PSG::Synth.new(@driver, voice_pools: []) }
    assert_raise(ArgumentError) { PSG::Synth.new(@driver, voice_pools: {mml: 0}) }
    assert_raise(ArgumentError) { PSG::Synth.new(@driver, voice_pools: {mml: 2, uart: 2}) }
    assert_raise(ArgumentError) { PSG::Synth.new(@driver, voice_pools: {mml: []}) }
    assert_raise(ArgumentError) { PSG::Synth.new(@driver, voice_pools: {mml: [0, 0]}) }
    assert_raise(ArgumentError) { PSG::Synth.new(@driver, voice_pools: {mml: [3]}) }
  end

  def test_drum_stays_in_source_pool
    use_synth({mml: 2, uart: 1})
    process([:note_on, 0, 48, 100], :mml)
    process([:note_on, 1, 52, 100], :mml)
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :uart, 100)
    assert_equal 2, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :uart)
    assert_not_nil @synth.allocator.voice_for(0, 48, source: :mml)
    assert_not_nil @synth.allocator.voice_for(1, 52, source: :mml)
  end

  def test_midi_channel_10_triggers_drum_with_reserved_voice
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :uart, 100)
    assert @driver.calls.include?([:voice_write, 0, 180, 4, 12, 3])
    assert_equal 0, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :uart)
  end

  def test_midi_channel_10_ignores_unknown_drum_note
    process([:note_on, PSG::DRUM_CHANNEL, 60, 100], :uart, 100)
    assert_equal [], @driver.calls
    assert_nil @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 60, source: :uart)
  end

  def test_midi_channel_10_steals_oldest_voice_for_drum_headroom
    process([:note_on, 0, 60, 100], :uart, 100)
    process([:note_on, 0, 62, 100], :uart, 100)
    process([:note_on, 0, 64, 100], :uart, 100)
    assert_equal [:uart, 0, 60, 100, 1], @synth.allocator.entry(0)
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :uart, 100)
    assert @driver.calls.include?([:mute, 0, 1, 0])
    assert @driver.calls.include?([:voice_write, 0, 180, 4, 12, 3])
    assert_equal 0, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :uart)
    assert_nil @synth.allocator.voice_for(0, 60, source: :uart)
  end

  def test_click_reclaims_fixed_voice_and_cancels_drum_program
    use_synth({looper_live: [0, 1, 2], looper_click: [2]})
    process([:note_on, 0, 60, 100], :looper_live, 0)
    process([:note_on, 0, 62, 100], :looper_live, 0)
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :looper_live, 0)
    assert_equal 2, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :looper_live)

    click_priority = PSG::Synth::DRUM_PRIORITY + 1
    process([:note_on, 15, 84, 127], :looper_click, click_priority)

    assert_equal 2, @synth.allocator.voice_for(15, 84, source: :looper_click)
    assert_nil @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :looper_live)
    period = PSG.note_to_period(84)
    assert @driver.calls.include?([:voice_write, 2, period, 0, 15, 1])
  end

  def test_drum_does_not_steal_active_click_voice
    use_synth({looper_live: [0, 1, 2], looper_click: [2]})
    click_priority = PSG::Synth::DRUM_PRIORITY + 1
    process([:note_on, 15, 84, 127], :looper_click, click_priority)
    process([:note_on, PSG::DRUM_CHANNEL, 38, 100], :looper_live, 0)

    assert_equal 2, @synth.allocator.voice_for(15, 84, source: :looper_click)
    assert_equal 0, @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 38, source: :looper_live)
    assert @driver.calls.include?([:voice_write, 0, 180, 4, 12, 3])
  end

  def test_midi_channel_10_uses_custom_program_assignment
    PSG.define_voice_program(:test_fire, [[120, 0, 15, 25]])
    PSG.assign_drum_program(60, :test_fire)
    process([:note_on, PSG::DRUM_CHANNEL, 60, 100], :uart, 100)
    assert @driver.calls.include?([:voice_write, 0, 120, 0, 12, 1])
  end

  def test_midi_channel_10_note_off_stops_drum_program
    process([:note_on, PSG::DRUM_CHANNEL, 46, 127], :uart, 100)
    voice = @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 46, source: :uart)
    assert_not_nil voice
    @driver.calls.clear

    process([:note_off, PSG::DRUM_CHANNEL, 46, 0], :uart, 100)

    assert_nil @synth.allocator.voice_for(PSG::DRUM_CHANNEL, 46, source: :uart)
    assert @driver.calls.include?([:voice_write, voice, 0, 0, 0, 0])
  end

  def test_named_voice_program_can_be_started_and_stopped
    PSG.define_voice_program(:test_effect, [[100, 2, 15, 100]])
    @synth.trigger_program(:test_effect, source: :game)
    Task.pass
    voice = @synth.allocator.voice_for(PSG::Synth::PROGRAM_CHANNEL, :test_effect.object_id, source: :game)
    assert_not_nil voice

    @synth.stop_program(:test_effect, source: :game)
    Task.pass
    assert_nil @synth.allocator.voice_for(PSG::Synth::PROGRAM_CHANNEL, :test_effect.object_id, source: :game)
  end

  def test_voice_program_advances_while_event_queue_is_idle
    PSG.define_voice_program(:test_timed, [[100, 0, 15, 8], [200, 0, 10, 40]])
    @synth.trigger_program(:test_timed, source: :game)
    Task.pass
    voice = @synth.allocator.voice_for(PSG::Synth::PROGRAM_CHANNEL, :test_timed.object_id, source: :game)
    assert @driver.calls.include?([:voice_write, voice, 100, 0, 15, 1])

    sleep_ms 20

    assert @driver.calls.include?([:voice_write, voice, 200, 0, 10, 1])
  end
end
