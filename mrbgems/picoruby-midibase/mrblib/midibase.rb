module MIDIBASE
  NOTE_OFF               = 0x80
  NOTE_ON                = 0x90
  POLYPHONIC_KEY_PRESSURE = 0xA0
  CONTROL_CHANGE         = 0xB0
  PROGRAM_CHANGE         = 0xC0
  CHANNEL_PRESSURE       = 0xD0
  PITCH_BEND             = 0xE0

  SYSTEM_EXCLUSIVE       = 0xF0
  TIME_CODE_QUARTER_FRAME = 0xF1
  SONG_POSITION          = 0xF2
  SONG_SELECT            = 0xF3
  TUNE_REQUEST           = 0xF6
  END_OF_EXCLUSIVE       = 0xF7
  TIMING_CLOCK           = 0xF8
  START                  = 0xFA
  CONTINUE               = 0xFB
  STOP                   = 0xFC
  ACTIVE_SENSING         = 0xFE
  SYSTEM_RESET           = 0xFF

  DEFAULT_MAX_SYSEX_BYTES = 1024

  CHANNEL_EVENTS = [
    :note_off, :note_on, :polyphonic_key_pressure, :control_change,
    :program_change, :channel_pressure, :pitch_bend
  ]
  TRANSPORT_EVENTS = [:timing_clock, :start, :continue, :stop, :system_reset]
  WIRE_EVENTS = CHANNEL_EVENTS + [
    :system_exclusive, :time_code_quarter_frame, :song_position,
    :song_select, :tune_request, :timing_clock, :start, :continue, :stop,
    :active_sensing, :system_reset
  ]

  def self.encode(command, *values)
    case command
    when :note_off
      channel_event(NOTE_OFF, values, 3)
    when :note_on
      channel_event(NOTE_ON, values, 3)
    when :polyphonic_key_pressure
      channel_event(POLYPHONIC_KEY_PRESSURE, values, 3)
    when :control_change
      channel_event(CONTROL_CHANGE, values, 3)
    when :program_change
      channel_event(PROGRAM_CHANGE, values, 2)
    when :channel_pressure
      channel_event(CHANNEL_PRESSURE, values, 2)
    when :pitch_bend
      require_values(values, 2)
      channel = channel_value(values[0])
      value = fourteen_bit_value(values[1])
      [PITCH_BEND | channel, value & 0x7F, (value >> 7) & 0x7F]
    when :system_exclusive
      require_values(values, 1)
      data = values[0]
      raise ArgumentError, "SysEx data must be a String" unless data.is_a?(String)
      bytes = [SYSTEM_EXCLUSIVE]
      i = 0
      while i < data.bytesize
        byte = data.getbyte(i) || 0
        data_value(byte)
        bytes << byte
        i += 1
      end
      bytes << END_OF_EXCLUSIVE
      bytes
    when :time_code_quarter_frame
      [TIME_CODE_QUARTER_FRAME, data_value(single_value(values))]
    when :song_position
      value = fourteen_bit_value(single_value(values))
      [SONG_POSITION, value & 0x7F, (value >> 7) & 0x7F]
    when :song_select
      [SONG_SELECT, data_value(single_value(values))]
    when :tune_request then no_value_event(values, TUNE_REQUEST)
    when :timing_clock then no_value_event(values, TIMING_CLOCK)
    when :start then no_value_event(values, START)
    when :continue then no_value_event(values, CONTINUE)
    when :stop then no_value_event(values, STOP)
    when :active_sensing then no_value_event(values, ACTIVE_SENSING)
    when :system_reset then no_value_event(values, SYSTEM_RESET)
    else
      raise ArgumentError, "unknown MIDI command: #{command}"
    end
  end

  def self.channel_event(status, values, count)
    require_values(values, count)
    bytes = [status | channel_value(values[0])]
    i = 1
    while i < count
      bytes << data_value(values[i])
      i += 1
    end
    bytes
  end

  def self.single_value(values)
    require_values(values, 1)
    values[0]
  end

  def self.no_value_event(values, status)
    require_values(values, 0)
    [status]
  end

  def self.require_values(values, count)
    raise ArgumentError, "wrong number of MIDI event values" unless values.size == count
  end

  def self.channel_value(value)
    unless value.is_a?(Integer) && 0 <= value && value <= 15
      raise ArgumentError, "MIDI channel must be in 0..15"
    end
    value
  end

  def self.data_value(value)
    unless value.is_a?(Integer) && 0 <= value && value <= 127
      raise ArgumentError, "MIDI data must be in 0..127"
    end
    value
  end

  def self.fourteen_bit_value(value)
    unless value.is_a?(Integer) && 0 <= value && value <= 16_383
      raise ArgumentError, "14-bit MIDI data must be in 0..16383"
    end
    value
  end

  def initialize_midibase(time_signature: [4, 4], max_sysex_bytes: DEFAULT_MAX_SYSEX_BYTES)
    @midi_parser = Parser.new(max_sysex_bytes: max_sysex_bytes)
    @midi_clock = Clock.new(time_signature: time_signature)
    @last_event_timestamp_us = nil
    self
  end

  attr_reader :last_event_timestamp_us

  def getevent
    parser = @midi_parser
    clock = @midi_clock
    raise "MIDI is not initialized" unless parser && clock
    while true
      byte = midi_read_byte
      if byte.nil?
        sleep_ms 1
        next
      end
      timestamp_us = midi_read_timestamp_us
      event = parser.feed(byte)
      next if event.nil?
      @last_event_timestamp_us = timestamp_us
      clock.observe(event, timestamp_us)
      return event
    end
    raise "unreachable"
  end

  def putevent(command, *values)
    bytes = ::MIDIBASE.encode(command, *values)
    bytes_size = bytes.size
    i = 0
    while i < bytes_size
      midi_write_byte(bytes[i])
      i += 1
    end
    bytes_size
  end

  def handle(event, **_context)
    command = event[0]
    raise ArgumentError, "MIDI event must start with a command Symbol" unless command.is_a?(Symbol)
    values = event[1..-1] || []
    putevent(command, *values)
  end

  def bpm
    @midi_clock&.bpm
  end

  def clock_running?
    @midi_clock&.clock_running?
  end

  def time_signature
    @midi_clock&.time_signature
  end

  def time_signature=(signature)
    clock = @midi_clock
    raise "MIDI is not initialized" unless clock
    clock.time_signature = signature
  end

  def position
    clock = @midi_clock
    clock ? clock.position : [1, 1, 0]
  end

  def bar
    position[0]
  end

  def beat
    position[1]
  end

  def tick
    position[2]
  end

  private def midi_read_timestamp_us
    Machine.uptime_us
  end
end
