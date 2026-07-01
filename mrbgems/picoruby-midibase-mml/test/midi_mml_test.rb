class MIDIBASEMMLCollector
  attr_reader :events, :sources
  def initialize
    @events = []
    @sources = []
  end
  def handle(event, source:, **_context)
    @events << event
    @sources << source
  end
end

class MIDIBASEMMLTest < Picotest::Test
  def collect(sequence)
    events = []
    absolute = 0
    while (item = sequence.next_event)
      absolute += item[0]
      events << [absolute, item[1]]
    end
    events
  end

  def find_command(events, command)
    i = 0
    while i < events.size
      return events[i] if events[i][1][0] == command
      i += 1
    end
    nil
  end

  def has_command?(events, command)
    !find_command(events, command).nil?
  end

  def find_notes(events, command)
    notes = []
    i = 0
    while i < events.size
      notes << events[i] if events[i][1][0] == command
      i += 1
    end
    notes
  end

  def test_sequence_emits_midi_and_psg_events_in_ticks
    sequence = MIDIBASE::MML::Sequence.new([
      "t120 o4 l4 q4 v8 p15 @2 z128 s3 m800 x2 y7 j2,5 c | r4 d"
    ])
    events = collect(sequence)
    assert has_command?(events, :tempo)
    assert has_command?(events, :program_change)
    assert has_command?(events, :pitch_bend)
    assert has_command?(events, :psg)
    assert has_command?(events, :barline)

    note_on = find_command(events, :note_on)
    note_off = find_command(events, :note_off)
    assert_equal [:note_on, 0, 60, 127], note_on[1]
    assert_equal 240, note_off[0] - note_on[0]
  end

  def test_rest_and_multiple_tracks_are_merged
    sequence = MIDIBASE::MML::Sequence.new(["l4 c d", "l2 r c"])
    events = collect(sequence)
    notes = []
    i = 0
    while i < events.size
      notes << events[i] if events[i][1][0] == :note_on
      i += 1
    end
    assert_equal 3, notes.size
    assert_equal 0, notes[0][0]
    assert_equal 480, notes[1][0]
    assert_equal 960, notes[2][0]
    assert_equal 1, notes[2][1][1]
  end

  def test_named_drums_emit_midi_channel_10_notes
    events = collect(MIDIBASE::MML::Sequence.new([
      "l8 !kick !snare !closed_hihat !open_hihat !low_tom !mid_tom !high_tom"
    ]))
    notes = find_notes(events, :note_on)
    assert_equal 7, notes.size
    assert_equal [35, 38, 42, 46, 41, 45, 48], notes.map { |item| item[1][2] }
    i = 0
    while i < notes.size
      assert_equal 9, notes[i][1][1]
      assert_equal i * 240, notes[i][0]
      i += 1
    end
  end

  def test_drum_aliases_are_case_insensitive_and_can_be_adjacent
    events = collect(MIDIBASE::MML::Sequence.new([
      "!BD8!SD8!CH8!OH8!LT8!MT8!HT8"
    ]))
    notes = find_notes(events, :note_on)
    assert_equal [35, 38, 42, 46, 41, 45, 48], notes.map { |item| item[1][2] }
  end

  def test_drum_length_and_gate_match_regular_notes
    events = collect(MIDIBASE::MML::Sequence.new(["q4 !kick4. c4"], channels: [3]))
    note_ons = find_notes(events, :note_on)
    note_offs = find_notes(events, :note_off)
    assert_equal [:note_on, 9, 35, 127], note_ons[0][1]
    assert_equal 360, note_offs[0][0] - note_ons[0][0]
    assert_equal 720, note_ons[1][0]
    assert_equal 3, note_ons[1][1][1]
  end

  def test_named_drums_work_inside_loops
    events = collect(MIDIBASE::MML::Sequence.new(["[!bd8 !sd8]2"]))
    notes = find_notes(events, :note_on)
    assert_equal [35, 38, 35, 38], notes.map { |item| item[1][2] }
  end

  def test_unknown_drum_obeys_exception_setting
    assert_raise(RuntimeError) do
      collect(MIDIBASE::MML::Sequence.new(["!cowbell4"]))
    end
    assert_raise(RuntimeError) do
      collect(MIDIBASE::MML::Sequence.new(["!4"]))
    end
    events = collect(MIDIBASE::MML::Sequence.new(["!cowbell4 c4"], exception: false))
    note = find_command(events, :note_on)
    assert_equal 0, note[0]
    assert_equal 60, note[1][2]
  end

  def test_barline_is_preserved_as_metadata
    events = collect(MIDIBASE::MML::Sequence.new(["c|d"]))
    barline = find_command(events, :barline)
    assert_equal [:barline, 1], barline[1]
  end

  def test_midi_clock_transport_and_generation
    clock = MIDIBASE::MML::MIDIClock.new
    assert_equal 0, clock.generation
    clock.handle([:start], timestamp_us: 1_000)
    assert_equal 1, clock.generation
    clock.handle([:timing_clock], timestamp_us: 2_000)
    clock.handle([:stop], timestamp_us: 3_000)
    clock.handle([:continue], timestamp_us: 4_000)
    assert_equal 1, clock.generation
    clock.handle([:system_reset], timestamp_us: 5_000)
    assert_equal 2, clock.generation
  end

  def test_internal_player_dispatches_sequence
    collector = MIDIBASEMMLCollector.new
    sequence = MIDIBASE::MML::Sequence.new(["t60000 l4 c"])
    player = MIDIBASE::MML::Player.new(sequence, output: collector).start
    player.join
    assert_equal MIDIBASE::MML::Player::SOURCE, collector.sources[0]
    wrapped = []
    i = 0
    while i < collector.events.size
      wrapped << [0, collector.events[i]]
      i += 1
    end
    assert has_command?(wrapped, :note_on)
    assert has_command?(wrapped, :note_off)
  end

  def test_external_clock_drives_player
    collector = MIDIBASEMMLCollector.new
    clock = MIDIBASE::MML::MIDIClock.new
    sequence = MIDIBASE::MML::Sequence.new(["l4 c"])
    player = MIDIBASE::MML::Player.new(
      sequence,
      clock: clock,
      output: collector
    ).start

    clock.handle([:start])
    Task.pass
    i = 0
    while i < 24
      clock.handle([:timing_clock])
      Task.pass
      i += 1
    end
    sleep_ms 2
    i = 0
    while i < 10
      found_note_off = false
      j = 0
      while j < collector.events.size
        found_note_off = true if collector.events[j][0] == :note_off
        j += 1
      end
      break if found_note_off
      Task.pass
      i += 1
    end
    player.stop.join

    wrapped = []
    i = 0
    while i < collector.events.size
      wrapped << [0, collector.events[i]]
      i += 1
    end
    assert has_command?(wrapped, :note_on)
    assert has_command?(wrapped, :note_off)
  end
end
