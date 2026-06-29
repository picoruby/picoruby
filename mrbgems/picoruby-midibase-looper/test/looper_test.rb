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
    looper.advance(10_000)
    assert_equal 1, events_for(looper.click_source, :note_off).size
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
end
