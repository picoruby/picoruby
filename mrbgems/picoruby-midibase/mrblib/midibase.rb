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

  class Parser
    def initialize(max_sysex_bytes: DEFAULT_MAX_SYSEX_BYTES)
      unless max_sysex_bytes.is_a?(Integer) && 0 < max_sysex_bytes
        raise ArgumentError, "max_sysex_bytes must be a positive Integer"
      end
      @max_sysex_bytes = max_sysex_bytes
      @running_status = nil
      @status = nil
      @expected = 0
      @data = []
      @sysex = nil
      @sysex_overflow = false
    end

    def reset
      @running_status = nil
      @status = nil
      @expected = 0
      @data.clear
      @sysex = nil
      @sysex_overflow = false
      self
    end

    def feed(byte)
      unless byte.is_a?(Integer) && 0 <= byte && byte <= 0xFF
        raise ArgumentError, "MIDI byte must be an Integer in 0..255"
      end

      if 0xF8 <= byte
        return realtime_event(byte)
      end

      if 0x80 <= byte
        return receive_status(byte)
      end

      if @sysex
        sysex = @sysex
        # @type var sysex: String
        if @sysex_overflow
          return nil
        elsif sysex.bytesize < @max_sysex_bytes
          sysex << byte
        else
          @sysex_overflow = true
        end
        return nil
      end

      if @status.nil?
        running_status = @running_status
        return nil if running_status.nil?
        begin_message(running_status)
      end

      @data << byte
      return nil if @data.size < @expected

      status = @status
      return nil if status.nil?
      event = build_event(status, @data)
      if status < 0xF0
        begin_message(status)
      else
        @status = nil
        @expected = 0
        @data.clear
      end
      event
    end

    private def receive_status(status)
      if @sysex && status != END_OF_EXCLUSIVE
        @sysex = nil
        @sysex_overflow = false
      end
      case status
      when SYSTEM_EXCLUSIVE
        @running_status = nil
        @status = nil
        @data.clear
        @sysex = ""
        @sysex_overflow = false
        nil
      when END_OF_EXCLUSIVE
        @running_status = nil
        @status = nil
        @data.clear
        sysex = @sysex
        return nil unless sysex

        event = if @sysex_overflow
                  [:system_exclusive_error, :too_large]
                else
                  [:system_exclusive, sysex]
                end
        @sysex = nil
        @sysex_overflow = false
        event
      when TUNE_REQUEST
        cancel_running_status
        [:tune_request]
      when 0xF1, 0xF2, 0xF3
        cancel_running_status
        begin_message(status)
        nil
      when 0xF4, 0xF5
        cancel_running_status
        nil
      else
        @sysex = nil
        @sysex_overflow = false
        @running_status = status
        begin_message(status)
        nil
      end
    end

    private def cancel_running_status
      @running_status = nil
      @status = nil
      @expected = 0
      @data.clear
    end

    private def begin_message(status)
      @status = status
      @data.clear
      high = status & 0xF0
      @expected = if high == PROGRAM_CHANGE || high == CHANNEL_PRESSURE || status == TIME_CODE_QUARTER_FRAME || status == SONG_SELECT
                    1
                  else
                    2
                  end
    end

    private def realtime_event(status)
      case status
      when TIMING_CLOCK   then [:timing_clock]
      when START          then [:start]
      when CONTINUE       then [:continue]
      when STOP           then [:stop]
      when ACTIVE_SENSING then [:active_sensing]
      when SYSTEM_RESET   then [:system_reset]
      else nil
      end
    end

    private def build_event(status, data)
      if status < 0xF0
        channel = status & 0x0F
        case status & 0xF0
        when NOTE_OFF
          [:note_off, channel, data[0], data[1]]
        when NOTE_ON
          if data[1] == 0
            [:note_off, channel, data[0], 0]
          else
            [:note_on, channel, data[0], data[1]]
          end
        when POLYPHONIC_KEY_PRESSURE
          [:polyphonic_key_pressure, channel, data[0], data[1]]
        when CONTROL_CHANGE
          [:control_change, channel, data[0], data[1]]
        when PROGRAM_CHANGE
          [:program_change, channel, data[0]]
        when CHANNEL_PRESSURE
          [:channel_pressure, channel, data[0]]
        when PITCH_BEND
          [:pitch_bend, channel, data[0] | (data[1] << 7)]
        end
      else
        case status
        when TIME_CODE_QUARTER_FRAME
          [:time_code_quarter_frame, data[0]]
        when SONG_POSITION
          [:song_position, data[0] | (data[1] << 7)]
        when SONG_SELECT
          [:song_select, data[0]]
        end
      end
    end
  end

  class Clock
    CLOCKS_PER_QUARTER_NOTE = 24

    attr_reader :bpm, :time_signature

    def initialize(time_signature: [4, 4])
      @bpm = nil
      @running = false
      @pulse = 0
      @sample_started_at = nil
      @sample_intervals = 0
      self.time_signature = time_signature
    end

    def time_signature=(signature)
      unless signature.is_a?(Array) && signature.size == 2
        raise ArgumentError, "time_signature must be [numerator, denominator]"
      end
      numerator = signature[0]
      denominator = signature[1]
      unless numerator.is_a?(Integer) && 0 < numerator &&
             denominator.is_a?(Integer) && 0 < denominator &&
             (denominator & (denominator - 1)) == 0 &&
             denominator <= 32
        raise ArgumentError, "invalid time signature"
      end
      @time_signature = [numerator, denominator]
      @clocks_per_beat = 96 / denominator
      @pulse = 0
      @time_signature
    end

    def observe(event, timestamp_us = nil)
      case event[0]
      when :timing_clock
        observe_timing_clock(timestamp_us || Machine.uptime_us)
      when :start
        @pulse = 0
        @running = true
        restart_tempo_sample
      when :continue
        @running = true
        restart_tempo_sample
      when :stop
        @running = false
      when :system_reset
        reset
      end
      event
    end

    def reset
      @bpm = nil
      @running = false
      @pulse = 0
      @sample_started_at = nil
      @sample_intervals = 0
      self
    end

    def clock_running?
      @running
    end

    def position
      clocks_per_bar = @time_signature[0] * @clocks_per_beat
      in_bar = @pulse % clocks_per_bar
      [@pulse / clocks_per_bar + 1, in_bar / @clocks_per_beat + 1, in_bar % @clocks_per_beat]
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

    private def observe_timing_clock(now_us)
      if @sample_started_at.nil?
        @sample_started_at = now_us
        @sample_intervals = 0
      else
        @sample_intervals += 1
        if CLOCKS_PER_QUARTER_NOTE <= @sample_intervals
          sample_started_at = @sample_started_at
          # @type var sample_started_at: Integer
          elapsed = now_us - sample_started_at
          @bpm = 60_000_000.0 / elapsed if 0 < elapsed
          @sample_started_at = now_us
          @sample_intervals = 0
        end
      end
      @pulse += 1 if @running
    end

    private def restart_tempo_sample
      @sample_started_at = nil
      @sample_intervals = 0
    end
  end

  class VoiceAllocator
    attr_reader :voice_count

    def initialize(voices: 3)
      unless voices.is_a?(Integer) && 0 < voices
        raise ArgumentError, "voices must be a positive Integer"
      end
      @voice_count = voices
      @voices = Array.new(voices)
      @age = 0
    end

    def allocate(channel, note)
      @age += 1
      voice = voice_for(channel, note)
      unless voice.nil?
        entry = @voices[voice]
        # @type var entry: Array[Integer]
        entry[2] = @age
        return voice
      end

      voice = @voices.index(nil)
      if voice.nil?
        voice = 0
        i = 1
        while i < @voices.size
          candidate = @voices[i]
          oldest = @voices[voice]
          # @type var candidate: Array[Integer]
          # @type var oldest: Array[Integer]
          voice = i if candidate[2] < oldest[2]
          i += 1
        end
      end
      @voices[voice] = [channel, note, @age]
      voice
    end

    def release(channel, note)
      voice = voice_for(channel, note)
      @voices[voice] = nil unless voice.nil?
      voice
    end

    def voice_for(channel, note)
      i = 0
      while i < @voices.size
        entry = @voices[i]
        return i if entry && entry[0] == channel && entry[1] == note
        i += 1
      end
      nil
    end

    def each_active
      i = 0
      while i < @voices.size
        entry = @voices[i]
        yield(i, entry[0], entry[1]) if entry
        i += 1
      end
      self
    end

    def release_all
      i = 0
      while i < @voices.size
        @voices[i] = nil
        i += 1
      end
      self
    end
  end

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
    self
  end

  def getevent
    initialize_midibase unless @midi_parser
    parser = @midi_parser
    clock = @midi_clock
    raise "MIDI is not initialized" unless parser && clock
    while true
      byte = midi_read_byte
      if byte.nil?
        sleep_ms 1
        next
      end
      event = parser.feed(byte)
      next if event.nil?
      clock.observe(event)
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

  def bpm
    @midi_clock&.bpm
  end

  def clock_running?
    @midi_clock&.clock_running? || false
  end

  def time_signature
    @midi_clock&.time_signature
  end

  def time_signature=(signature)
    initialize_midibase unless @midi_clock
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
end
