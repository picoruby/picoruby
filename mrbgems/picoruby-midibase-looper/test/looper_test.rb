class MIDIBASELooperFakeTime
  attr_accessor :now
  def initialize
    @now = 0
  end
  def uptime_us
    @now
  end
end

class MIDIBASELooperOutput
  attr_reader :events
  def initialize
    @events = []
  end
  def emit(source, event, timestamp_us: nil)
    @events << [source, event, timestamp_us]
    event
  end
end

class MIDIBASELooperInput
  def getevent
    [:note_on, 0, 60, 100]
  end
end

class MIDIBASELooperTimestampedInput
  attr_reader :last_event_timestamp_us

  def initialize
    @last_event_timestamp_us = 123_456
  end

  def getevent
    [:note_on, 0, 60, 100]
  end
end

class MIDIBASELooperStoppingOutput < MIDIBASELooperOutput
  def emit(source, event, timestamp_us: nil)
    super(source, event, timestamp_us: timestamp_us)
    raise "stop after first event"
  end
end

class MIDIBASELooperTest < Picotest::Test
  def setup
    @time = MIDIBASELooperFakeTime.new
    @output = MIDIBASELooperOutput.new
  end

  def build_looper(**options)
    defaults = {
      output: @output,
      time_source: @time,
      bars: 1,
      count_in_bars: 0,
      quantize: :off,
      metronome: :off
    }
    options.each do |key, value|
      defaults[key] = value
    end
    MIDIBASE::Looper.new(**defaults)
  end

  def events_for(source, command = nil)
    result = []
    i = 0
    events = @output.events
    events_size = events.size
    while i < events_size
      item = events[i]
      if item[0] == source && (command.nil? || item[1][0] == command)
        result << item
      end
      i += 1
    end
    result
  end

  def test_event_buffer_sorts_note_off_before_note_on
    buffer = MIDIBASE::Looper::EventBuffer.new(4)
    buffer.append(20, :note_on, 0, 62, 100)
    buffer.append(10, :note_on, 0, 60, 100)
    buffer.append(10, :note_off, 0, 59, 0)
    buffer.sort!
    assert_equal [:note_off, 0, 59, 0], buffer.event_at(0)
    assert_equal [:note_on, 0, 60, 100], buffer.event_at(1)
    assert_equal 20, buffer.tick_at(2)
  end

  def test_event_buffer_capacity_clear_and_validation
    buffer = MIDIBASE::Looper::EventBuffer.new(1)
    assert_equal 0, buffer.append(65_535, :note_on, 15, 127, 127)
    assert_nil buffer.append(0, :note_off, 0, 0, 0)
    assert_equal 1, buffer.count
    assert_equal 65_535, buffer.tick_at(0)
    assert_equal [:note_on, 15, 127, 127], buffer.event_at(0)

    buffer.clear
    assert_equal 0, buffer.count
    assert_raise(ArgumentError) { buffer.append(-1, :note_on, 0, 60, 100) }
    assert_raise(ArgumentError) { buffer.append(0, :control_change, 0, 60, 100) }
    assert_raise(ArgumentError) { buffer.append(0, :note_on, 16, 60, 100) }
  end

  def test_default_recording_quantizes_to_sixteenth_notes
    looper = MIDIBASE::Looper.new(output: @output, time_source: @time)
    assert_equal :sixteenth, looper.status[:quantize]
  end

  def test_input_pump_uses_high_priority_task
    pump = MIDIBASE::Looper::InputPump.new(
      MIDIBASELooperInput.new,
      output: @output,
      source: :midi_in
    ).start
    task = pump.instance_variable_get(:@task)
    assert_equal MIDIBASE::Looper::InputPump::DEFAULT_PRIORITY, task.priority
  ensure
    pump&.stop
  end

  def test_midi_queue_recycles_message_envelope
    looper = build_looper
    looper.instance_variable_set(:@task, true)
    free_messages = looper.instance_variable_get(:@free_midi_messages)
    initial_size = free_messages.size
    looper.handle_midi([:note_on, 0, 60, 100], :midi_in, 0, 123)
    assert_equal initial_size - 1, free_messages.size
    looper.send(:drain_queue)
    assert_equal initial_size, free_messages.size
  ensure
    looper&.instance_variable_set(:@task, nil)
  end

  def test_track_caches_current_event_until_advance
    buffer = MIDIBASE::Looper::EventBuffer.new(2)
    buffer.append(0, :note_on, 0, 60, 100)
    buffer.append(10, :note_off, 0, 60, 0)
    track = MIDIBASE::Looper::Track.new(buffer, source: :track, voice_limit: 1)
    first = track.current_event
    assert_equal first.object_id, track.current_event.object_id
    track.advance(20)
    assert_not_equal first.object_id, track.current_event.object_id
  end

  def test_input_pump_preserves_captured_input_timestamp
    output = MIDIBASELooperStoppingOutput.new
    pump = MIDIBASE::Looper::InputPump.new(
      MIDIBASELooperTimestampedInput.new,
      output: output,
      source: :midi_in
    )
    assert_raise(RuntimeError) { pump.run }
    assert_equal 123_456, output.events[0][2]
  end

  def test_quantize_wraps_end_to_tick_zero
    recorder = MIDIBASE::Looper::Recorder.new(
      loop_ticks: 1920,
      grid_ticks: 120,
      voice_limit: 1,
      max_events: 8
    )
    recorder.note_on(1880, 0, 60, 100)
    recorder.note_off(1910, 0, 60, 0)
    buffer = recorder.finish
    assert_not_nil buffer
    assert_equal 0, buffer.tick_at(0)
    assert_equal 30, buffer.tick_at(1)
  end

  def test_overflow_discards_recording
    recorder = MIDIBASE::Looper::Recorder.new(
      loop_ticks: 1920,
      grid_ticks: nil,
      voice_limit: 1,
      max_events: 1
    )
    recorder.note_on(0, 0, 60, 100)
    recorder.note_off(10, 0, 60, 0)
    assert recorder.overflow?
    assert_nil recorder.finish
  end

  def test_records_one_loop_and_replays_from_tick_zero
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 250_000)
    @time.now = 2_000_000
    looper.advance(@time.now)

    assert_equal :playing, looper.state
    assert_equal 1, looper.tracks.size
    track_source = looper.tracks[0].source
    assert_equal 1, events_for(track_source, :note_on).size

    @time.now = 2_250_000
    looper.advance(@time.now)
    assert_equal 1, events_for(track_source, :note_off).size
  end

  def test_queued_events_after_recording_boundary_are_recorded
    looper = build_looper(count_in_bars: 1)
    looper.record(voices: 1)
    looper.instance_variable_set(:@task, true)

    looper.handle([:note_on, 0, 60, 100], timestamp_us: 2_050_000)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 2_100_000)
    looper.send(:drain_queue)

    recorder = looper.instance_variable_get(:@recorder)
    assert_equal :recording, looper.state
    assert_equal 2, recorder.buffer.count
    assert_equal 48, recorder.buffer.tick_at(0)
    assert_equal 96, recorder.buffer.tick_at(1)
  ensure
    looper&.instance_variable_set(:@task, nil)
  end

  def test_track_priority_sources_are_distinct_from_live
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    assert_not_equal MIDIBASE::Looper::LIVE_SOURCE, looper.tracks[0].source
  end

  def test_mute_sends_note_off_immediately
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 1_000_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    track_source = looper.tracks[0].source
    before = events_for(track_source, :note_off).size
    @time.now = 2_100_000
    looper.mute(0)
    after = events_for(track_source, :note_off).size
    assert_equal before + 1, after
  end

  def test_first_take_reserves_click_voice
    looper = build_looper(count_in_bars: 1, metronome: :count_in)
    assert_raise(ArgumentError) { looper.record(voices: 3) }
    looper.record(voices: 2)
    assert_equal :count_in, looper.state
  end

  def test_count_in_emits_click_and_note_off
    looper = build_looper(count_in_bars: 1, metronome: :count_in)
    looper.record(voices: 2)
    looper.advance(0)
    assert_equal 1, events_for(looper.click_source, :note_on).size
    looper.advance(MIDIBASE::Looper::CLICK_DURATION_US)
    assert_equal 1, events_for(looper.click_source, :note_off).size
  end

  def test_first_take_click_continues_until_recording_finishes
    looper = build_looper(count_in_bars: 1, metronome: :count_in)
    looper.record(voices: 2)

    looper.advance(0)
    looper.advance(500_000)
    looper.advance(1_000_000)
    looper.advance(1_500_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    assert_equal :recording, looper.state
    count_at_recording_start = events_for(looper.click_source, :note_on).size
    assert_equal 5, count_at_recording_start

    @time.now = 2_500_000
    looper.advance(@time.now)
    assert_equal count_at_recording_start + 1, events_for(looper.click_source, :note_on).size

    @time.now = 3_000_000
    looper.advance(@time.now)
    @time.now = 3_500_000
    looper.advance(@time.now)
    count_before_finish = events_for(looper.click_source, :note_on).size
    assert_equal count_at_recording_start + 3, count_before_finish

    @time.now = 4_000_000
    looper.advance(@time.now)
    assert_equal :playing, looper.state
    assert_equal count_before_finish, events_for(looper.click_source, :note_on).size
  end

  def test_first_take_without_count_in_still_reserves_click_voice
    looper = build_looper(count_in_bars: 0, metronome: :count_in)
    assert_raise(ArgumentError) { looper.record(voices: 3) }
    looper.record(voices: 2)
    looper.advance(0)
    click = events_for(looper.click_source, :note_on)[0]
    assert_equal MIDIBASE::Looper::CLICK_VELOCITY, click[1][3]
  end

  def test_latest_note_priority_is_recorded
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_on, 0, 62, 110], timestamp_us: 100_000)
    looper.handle([:note_off, 0, 62, 0], timestamp_us: 200_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    events = looper.tracks[0].events
    assert_equal 4, events.count
    assert_equal [:note_off, 0, 60, 0], events.event_at(1)
    assert_equal [:note_on, 0, 62, 110], events.event_at(2)
  end

  def test_second_track_records_at_next_loop_boundary
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)

    @time.now = 2_100_000
    looper.record(voices: 1)
    assert_equal :armed, looper.state
    @time.now = 4_000_000
    looper.advance(@time.now)
    assert_equal :recording, looper.state
    looper.handle([:note_on, 1, 64, 90], timestamp_us: 4_000_000)
    looper.handle([:note_off, 1, 64, 0], timestamp_us: 4_100_000)
    @time.now = 6_000_000
    looper.advance(@time.now)
    assert_equal 2, looper.tracks.size
  end

  def test_continuous_metronome_requires_a_voice
    looper = build_looper
    looper.record(voices: 3)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    assert_raise(RuntimeError) { looper.metronome = :beat }
  end

  def test_meter_and_bars_lock_after_recording
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    assert_raise(RuntimeError) { looper.time_signature = [3, 4] }
    assert_raise(RuntimeError) { looper.bars = 2 }
  end

  def test_default_part_a_preserves_legacy_track_view
    looper = build_looper
    status = looper.status
    assert_equal :A, status[:selected_part]
    assert_equal 1, status[:parts].size
    assert_equal status[:tracks], status[:parts][0][:tracks]
  end

  def test_part_copy_shares_sealed_events_but_has_independent_track_state
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    original = looper.tracks[0]

    copied = looper.copy_part(:A, :B)
    assert_equal :B, copied.id
    assert_equal original.events.object_id, looper.tracks[0].events.object_id
    assert looper.tracks[0].events.sealed?
    assert_raise(RuntimeError) { looper.tracks[0].events.clear }

    looper.mute(0)
    assert looper.tracks[0].muted
    looper.select_part(:A)
    assert_false looper.tracks[0].muted
  end

  def test_records_selected_part_at_active_part_boundary
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)

    looper.create_part(:B, bars: 2)
    @time.now = 2_100_000
    looper.record(voices: 1)
    assert_equal :B, looper.status[:queued_part]
    @time.now = 4_000_000
    looper.advance(@time.now)
    assert_equal :recording, looper.state
    assert_equal :B, looper.status[:active_part]
    looper.handle([:note_on, 1, 64, 90], timestamp_us: 4_000_000)
    looper.handle([:note_off, 1, 64, 0], timestamp_us: 4_100_000)
    @time.now = 8_000_000
    looper.advance(@time.now)

    assert_equal [2], looper.status[:tracks][0][:channels]
    looper.select_part(:A)
    assert_equal [1], looper.status[:tracks][0][:channels]
  end

  def test_arrangement_repeats_parts_and_stops_after_last_step
    looper = build_looper
    looper.create_part(:B)
    looper.create_part(:C)
    looper.arrangement = [[:A, 2], [:B, 4], [:A, 2], [:C, 1]]
    looper.play_song

    @time.now = 4_000_000
    looper.advance(@time.now)
    assert_equal :B, looper.status[:active_part]
    assert_equal 1, looper.status[:song_step]

    @time.now = 12_000_000
    looper.advance(@time.now)
    assert_equal :A, looper.status[:active_part]
    @time.now = 16_000_000
    looper.advance(@time.now)
    assert_equal :C, looper.status[:active_part]
    @time.now = 18_000_000
    looper.advance(@time.now)
    assert_equal :stopped, looper.state
  end

  def test_part_delete_cascades_arrangement_and_undo_restores_it
    looper = build_looper
    looper.create_part(:B)
    looper.arrangement = [[:A, 1], [:B, 2]]
    looper.delete_part(:B)
    assert_equal 1, looper.status[:parts].size
    assert_equal 1, looper.status[:arrangement].size

    looper.undo
    assert_equal 2, looper.status[:parts].size
    assert_equal :B, looper.status[:arrangement][1][:part]
    looper.redo
    assert_equal 1, looper.status[:parts].size
  end

  def test_recording_undo_and_redo_work_during_playback
    looper = build_looper
    looper.record(voices: 1)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    assert_equal 1, looper.tracks.size

    looper.undo
    assert_equal 0, looper.tracks.size
    looper.redo
    assert_equal 1, looper.tracks.size
  end

  def test_voice_budget_is_scoped_to_selected_part
    looper = build_looper(voice_capacity: 3)
    looper.record(voices: 2)
    looper.handle([:note_on, 0, 60, 100], timestamp_us: 0)
    looper.handle([:note_off, 0, 60, 0], timestamp_us: 100_000)
    @time.now = 2_000_000
    looper.advance(@time.now)
    assert_equal 1, looper.status[:available_voices]

    looper.create_part(:B)
    assert_equal 3, looper.status[:available_voices]
  end

  def test_midi_click_does_not_consume_a_music_voice
    looper = build_looper(
      voice_capacity: 3,
      click_voice_cost: 0,
      count_in_bars: 1,
      metronome: :count_in
    )
    looper.record(voices: 3)
    assert_equal :count_in, looper.state
  end

  def test_cli_uses_sixteen_voices_and_free_click_for_midi_only
    options = LooperCLIOptions.new
    options.parse(["--audio", "off", "--midi-out", "uart"])
    assert_equal 16, options.polyphony
    assert_equal 0, options.click_voice_cost
    assert_equal :midi, options.click_out
  end

  def test_cli_accepts_usb_cdc_midi_output
    options = LooperCLIOptions.new
    options.parse(["--audio", "off", "--midi-out", "cdc"])
    assert_equal :cdc, options.midi_out
    assert options.midi_thru
    assert_equal 16, options.polyphony
    assert_equal 0, options.click_voice_cost
    assert_equal :midi, options.click_out
  end

  def test_cli_can_disable_midi_thru_for_usb_cdc_output
    options = LooperCLIOptions.new
    options.parse(["--audio", "off", "--midi-out", "cdc", "--no-midi-thru"])
    assert_false options.midi_thru
  end

  def test_cli_rejects_more_than_three_psg_voices
    options = LooperCLIOptions.new
    assert_raise(ArgumentError) { options.parse(["--polyphony", "4"]) }
  end
end
