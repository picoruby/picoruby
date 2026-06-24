require "midibase"

module PSG
  class Synth
    DEFAULT_PITCH_BEND_RANGE = 2
    WAIT_RETRY_MS = 1

    attr_reader :allocator

    def initialize(driver, voices: 3)
      unless voices.is_a?(Integer) && 0 < voices && voices <= 3
        raise ArgumentError, "PSG voices must be in 1..3"
      end
      @driver = driver
      @allocator = ::MIDIBASE::VoiceAllocator.new(voices: voices)
      @velocities = Array.new(voices, 0)
      @states = {}
      @sustained = {}
      @queue = Task::Queue.new
      @mixer = 0b111000
      @stopped = false
      @task = nil
    end

    def start
      synth = self
      @task = Task.new(name: "PSG::Synth") { synth.run }
      self
    end

    def handle(event, source: nil, priority: 0, timestamp_us: nil, **_context)
      return false if @stopped

      return false if @queue.closed?
      @queue.push([event, source, priority, timestamp_us])
      true
    rescue Task::Error
      false
    end

    def stop
      return self if @stopped
      @stopped = true
      @queue.close unless @queue.closed?
      silence_all
      self
    end

    def join
      @task&.join
      self
    rescue RuntimeError
      Task.run
      self
    end

    def run
      while !@stopped
        envelope = @queue.pop
        break if envelope.nil?
        process(envelope[0], envelope[1], envelope[2])
      end
    ensure
      silence_all
    end

    private def process(event, source, priority)
      command = event[0]
      channel = event[1]
      case command
      when :note_on
        note_on(source, channel, event[2], event[3], priority)
      when :note_off
        note_off(source, channel, event[2])
      when :pitch_bend
        pitch_bend(source, channel, event[2])
      when :control_change
        control_change(source, channel, event[2], event[3])
      when :program_change
        program_change(source, channel, event[2])
      when :psg
        psg_event(source, channel, event)
      end
    end

    private def note_on(source, channel, note, velocity, priority)
      old_voice = @allocator.voice_for(channel, note, source: source)
      voice = @allocator.allocate(channel, note, source: source, priority: priority)
      return if voice.nil?
      stolen = @allocator.last_stolen
      if stolen || old_voice
        write_driver(:mute, voice, 1, 0)
        if stolen
          @sustained.delete(note_key(stolen[0], stolen[1], stolen[2]))
        end
      end
      @velocities[voice] = velocity
      @sustained.delete(note_key(source, channel, note))
      state = state_for(source, channel)
      write_period(voice, note, state)
      write_driver(:set_timbre, voice, state[8], 0)
      write_driver(:set_pan, voice, midi_pan(state), 0)
      write_driver(:set_legato, voice, state[9], 0)
      write_driver(:set_lfo, voice, state[10], state[11], 0)
      apply_mixer(voice, state[12])
      write_driver(:send_reg, voice + 8, voice_volume(voice, state), 0)
      write_driver(:mute, voice, 0, 0)
    end

    private def note_off(source, channel, note)
      voice = @allocator.voice_for(channel, note, source: source)
      return if voice.nil?
      state = state_for(source, channel)
      if state[4]
        @sustained[note_key(source, channel, note)] = true
      else
        release_voice(voice, source, channel, note)
      end
    end

    private def pitch_bend(source, channel, value)
      state = state_for(source, channel)
      state[3] = value
      update_periods(source, channel, state)
    end

    private def program_change(source, channel, program)
      state = state_for(source, channel)
      state[8] = program & 0x03
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        write_driver(:set_timbre, i, state[8], 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def control_change(source, channel, controller, value)
      state = state_for(source, channel)
      case controller
      when 1
        state[10] = value * 100
        update_lfo(source, channel, state)
      when 7
        state[0] = value
        state[13] = false
        update_volumes(source, channel, state)
      when 10
        state[2] = value
        update_pan(source, channel, state)
      when 11
        state[1] = value
        update_volumes(source, channel, state)
      when 64
        sustain_change(source, channel, state, 64 <= value)
      when 68
        state[9] = 64 <= value ? 1 : 0
        update_legato(source, channel, state)
      when 100
        state[6] = value
      when 101
        state[5] = value
      when 6
        if state[5] == 0 && state[6] == 0
          state[7] = value
          update_periods(source, channel, state)
        end
      when 120
        all_notes_off(source, channel, true)
      when 123
        all_notes_off(source, channel, false)
      end
    end

    private def psg_event(source, channel, event)
      state = state_for(source, channel)
      case event[2]
      when :env_period
        value = event[3]
        write_driver(:send_reg, 11, value & 0xFF, 0)
        write_driver(:send_reg, 12, value >> 8, 0)
      when :env_shape
        state[13] = true
        write_driver(:send_reg, 13, event[3], 0)
        update_volumes(source, channel, state)
      when :lfo
        state[10] = event[3]
        state[11] = event[4]
        update_lfo(source, channel, state)
      when :mixer
        state[12] = event[3]
        update_mixer(source, channel, state)
      when :noise
        write_driver(:send_reg, 6, event[3], 0)
      end
    end

    private def state_for(source, channel)
      key = [source, channel]
      state = @states[key]
      unless state
        state = [127, 127, 64, 8192, false, 127, 127,
                 DEFAULT_PITCH_BEND_RANGE, 0, 0, 0, 0, 0, false]
        @states[key] = state
      end
      state
    end

    private def sustain_change(source, channel, state, enabled)
      was_enabled = state[4]
      state[4] = enabled
      return if enabled || !was_enabled
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        if entry && owned_by?(entry, source, channel) && @sustained[note_key(source, channel, entry[2])]
          release_voice(i, source, channel, entry[2])
        end
        i += 1
      end
    end

    private def all_notes_off(source, channel, immediate)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        if entry && owned_by?(entry, source, channel)
          state = state_for(source, channel)
          if immediate || !state[4]
            release_voice(i, source, channel, entry[2])
          else
            @sustained[note_key(source, channel, entry[2])] = true
          end
        end
        i += 1
      end
    end

    private def release_voice(voice, source, channel, note)
      write_driver(:mute, voice, 1, 0)
      @allocator.release(channel, note, source: source)
      @velocities[voice] = 0
      @sustained.delete(note_key(source, channel, note))
    end

    private def update_periods(source, channel, state)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        write_period(i, entry[2], state) if entry && owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_volumes(source, channel, state)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        write_driver(:send_reg, i + 8, voice_volume(i, state), 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_pan(source, channel, state)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        write_driver(:set_pan, i, midi_pan(state), 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_legato(source, channel, state)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        write_driver(:set_legato, i, state[9], 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_lfo(source, channel, state)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        write_driver(:set_lfo, i, state[10], state[11], 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_mixer(source, channel, state)
      i = 0
      while i < @allocator.voice_count
        entry = @allocator.entry(i)
        apply_mixer(i, state[12]) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def apply_mixer(voice, mode)
      case mode
      when 0
        @mixer |= 1 << (voice + 3)
        @mixer &= ~(1 << voice)
      when 1
        @mixer &= ~(1 << (voice + 3))
        @mixer |= 1 << voice
      when 2
        @mixer &= ~(1 << voice)
        @mixer &= ~(1 << (voice + 3))
      end
      write_driver(:send_reg, 7, @mixer, 0)
    end

    private def voice_volume(voice, state)
      return 16 if state[13]
      volume = state[0]
      expression = state[1]
      # @type var volume: Integer
      # @type var expression: Integer
      numerator = @velocities[voice] * volume * expression * 15
      denominator = 127 * 127 * 127
      value = (numerator + denominator / 2) / denominator
      value = 1 if 0 < @velocities[voice] && value == 0
      value
    end

    private def midi_pan(state)
      (state[2] * 15 + 63) / 127
    end

    private def write_period(voice, note, state)
      bend_range = state[7]
      semitones = (state[3] - 8192) * bend_range.to_f / 8192.0
      period = PSG.note_to_period(note + semitones)
      write_driver(:send_reg, voice * 2, period & 0xFF, 0)
      write_driver(:send_reg, voice * 2 + 1, period >> 8 & 0x0F, 0)
    end

    private def owned_by?(entry, source, channel)
      entry && entry[0] == source && entry[1] == channel
    end

    private def note_key(source, channel, note)
      [source, channel, note]
    end

    private def silence_all
      @allocator.release_all
      i = 0
      while i < @allocator.voice_count
        @driver.mute_direct(i, 1)
        @velocities[i] = 0
        i += 1
      end
      @driver.buffer_flush
    end

    private def write_driver(command, *args)
      while !@stopped
        pushed = case command
                 when :send_reg
                   @driver.send_reg(args[0], args[1], args[2])
                 when :mute
                   @driver.mute(args[0], args[1], args[2])
                 when :set_pan
                   @driver.set_pan(args[0], args[1], args[2])
                 when :set_timbre
                   @driver.set_timbre(args[0], args[1], args[2])
                 when :set_legato
                   @driver.set_legato(args[0], args[1], args[2])
                 when :set_lfo
                   @driver.set_lfo(args[0], args[1], args[2], args[3])
                 else
                   raise ArgumentError, "unknown PSG command: #{command}"
                 end
        return true if pushed
        GC.start
        sleep_ms WAIT_RETRY_MS
      end
      false
    end
  end
end
