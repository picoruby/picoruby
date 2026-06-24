require "midibase"

module PSG
  class MIDIPlayer
    DEFAULT_PITCH_BEND_RANGE = 2
    MIDI_CHANNELS = 16

    attr_reader :allocator

    def initialize(driver, voices: 3, channels: nil, pitch_bend_range: DEFAULT_PITCH_BEND_RANGE)
      unless 0 < voices && voices <= 3
        raise ArgumentError, "PSG voices must be in 1..3"
      end
      if channels
        channels.each do |channel|
          unless channel.is_a?(Integer) && 0 <= channel && channel < MIDI_CHANNELS
            raise ArgumentError, "MIDI channels must be in 0..15"
          end
        end
      end
      unless 0 < pitch_bend_range
        raise ArgumentError, "pitch_bend_range must be positive"
      end

      @driver = driver
      @allocator = ::MIDIBASE::VoiceAllocator.new(voices: voices)
      @channels = channels
      @pitch_bend_range = pitch_bend_range
      @velocities = Array.new(voices, 0)
      @volume = Array.new(MIDI_CHANNELS, 127)
      @expression = Array.new(MIDI_CHANNELS, 127)
      @pan = Array.new(MIDI_CHANNELS, 64)
      @bend = Array.new(MIDI_CHANNELS, 8192)
      @sustain = Array.new(MIDI_CHANNELS, false)
      @sustained_notes = {}
    end

    def handle(event)
      command = event[0]
      channel = event[1]
      return self if channel.is_a?(Integer) && !accept_channel?(channel)

      case command
      when :note_on
        note_on(channel, event[2], event[3])
      when :note_off
        note_off(channel, event[2])
      when :pitch_bend
        pitch_bend(channel, event[2])
      when :control_change
        control_change(channel, event[2], event[3])
      end
      self
    end

    def all_notes_off(channel = nil, immediate: true)
      releases = [] #: Array[Array[Integer]]
      @allocator.each_active do |voice, active_channel, note|
        if channel.nil? || channel == active_channel
          releases << [voice, active_channel, note]
        end
      end
      releases.each do |entry|
        voice = entry[0]
        active_channel = entry[1]
        note = entry[2]
        if immediate || !@sustain[active_channel]
          write_driver(:mute, voice, 1, 0)
          @allocator.release(active_channel, note)
          @velocities[voice] = 0
        else
          @sustained_notes[note_key(active_channel, note)] = true
        end
      end
      self
    end

    private def accept_channel?(channel)
      channels = @channels
      channels.nil? || channels.include?(channel)
    end

    private def note_on(channel, note, velocity)
      voice = @allocator.allocate(channel, note)
      @sustained_notes.delete(note_key(channel, note))
      @velocities[voice] = velocity
      write_driver(:mute, voice, 1, 0)
      write_period(voice, note, channel)
      write_driver(:send_reg, voice + 8, voice_volume(voice, channel), 0)
      write_driver(:set_pan, voice, midi_pan(channel), 0)
      write_driver(:mute, voice, 0, 0)
    end

    private def note_off(channel, note)
      voice = @allocator.voice_for(channel, note)
      return if voice.nil?
      if @sustain[channel]
        @sustained_notes[note_key(channel, note)] = true
      else
        write_driver(:mute, voice, 1, 0)
        @allocator.release(channel, note)
        @velocities[voice] = 0
      end
    end

    private def pitch_bend(channel, value)
      @bend[channel] = value
      @allocator.each_active do |voice, active_channel, note|
        write_period(voice, note, active_channel) if active_channel == channel
      end
    end

    private def control_change(channel, controller, value)
      case controller
      when 7
        @volume[channel] = value
        update_channel_volumes(channel)
      when 10
        @pan[channel] = value
        update_channel_pan(channel)
      when 11
        @expression[channel] = value
        update_channel_volumes(channel)
      when 64
        sustain_change(channel, value)
      when 120
        all_notes_off(channel, immediate: true)
      when 123
        all_notes_off(channel, immediate: false)
      end
    end

    private def sustain_change(channel, value)
      enabled = 64 <= value
      was_enabled = @sustain[channel]
      @sustain[channel] = enabled
      return if enabled || !was_enabled

      releases = [] #: Array[Array[Integer]]
      @allocator.each_active do |voice, active_channel, note|
        if active_channel == channel && @sustained_notes[note_key(channel, note)]
          releases << [voice, note]
        end
      end
      releases.each do |entry|
        voice = entry[0]
        note = entry[1]
        write_driver(:mute, voice, 1, 0)
        @allocator.release(channel, note)
        @velocities[voice] = 0
        @sustained_notes.delete(note_key(channel, note))
      end
    end

    private def update_channel_volumes(channel)
      @allocator.each_active do |voice, active_channel, _note|
        if active_channel == channel
          write_driver(:send_reg, voice + 8, voice_volume(voice, channel), 0)
        end
      end
    end

    private def update_channel_pan(channel)
      @allocator.each_active do |voice, active_channel, _note|
        write_driver(:set_pan, voice, midi_pan(channel), 0) if active_channel == channel
      end
    end

    private def voice_volume(voice, channel)
      numerator = @velocities[voice] * @volume[channel] * @expression[channel] * 15
      denominator = 127 * 127 * 127
      value = (numerator + denominator / 2) / denominator
      value = 1 if 0 < @velocities[voice] && value == 0
      value
    end

    private def midi_pan(channel)
      (@pan[channel] * 15 + 63) / 127
    end

    private def write_period(voice, note, channel)
      semitones = (@bend[channel] - 8192) * @pitch_bend_range.to_f / 8192.0
      period = PSG.note_to_period(note + semitones)
      write_driver(:send_reg, voice * 2, period & 0xFF, 0)
      write_driver(:send_reg, voice * 2 + 1, (period >> 8) & 0x0F, 0)
    end

    private def note_key(channel, note)
      (channel << 7) | note
    end

    private def write_driver(command, *args)
      pushed = false
      until pushed
        pushed = case command
                 when :send_reg
                   @driver.send_reg(args[0], args[1], args[2])
                 when :mute
                   @driver.mute(args[0], args[1], args[2])
                 when :set_pan
                   @driver.set_pan(args[0], args[1], args[2])
                 else
                   raise ArgumentError, "unknown PSG command: #{command}"
                 end
        break if pushed
        GC.start
        sleep_ms 1
      end
      true
    end
  end
end
