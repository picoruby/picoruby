require "midibase"

module PSG
  class Synth
    DEFAULT_PITCH_BEND_RANGE = 2
    DRUM_PRIORITY = 1_000_000
    WAIT_RETRY_MS = 1

    attr_reader :allocator

    def initialize(driver, voice_count: 3, voice_pools: nil)
      unless 0 < voice_count && voice_count <= 3
        raise ArgumentError, "PSG voice_count must be in 1..3"
      end
      @driver = driver
      @allocator = ::MIDIBASE::VoiceAllocator.new(voice_count)
      @voice_pools = build_voice_pools(voice_pools, voice_count)
      @velocities = Array.new(voice_count, 0)
      @states = {}
      @sustained = {}
      @drum_voice = nil
      @drum_until_us = 0
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

      queue = @queue
      return false if queue.closed?
      queue.push([event, source, priority, timestamp_us])
      true
    rescue Task::Error
      false
    end

    def stop
      return self if @stopped
      @stopped = true
      queue = @queue
      queue.close unless queue.closed?
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
      queue = @queue
      while !@stopped
        envelope = queue.pop
        break if envelope.nil?
        process(envelope[0], envelope[1], envelope[2], envelope[3])
      end
    ensure
      silence_all
    end

    private def process(event, source, priority, timestamp_us)
      release_expired_drum_voice(timestamp_us)
      command = event[0]
      channel = event[1]
      case command
      when :note_on
        note_on(source, channel, event[2], event[3], priority, timestamp_us)
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
      when :psg_global
        psg_global_event(event)
      end
    end

    private def note_on(source, channel, note, velocity, priority, timestamp_us)
      if channel == PSG::DRUM_CHANNEL
        return if velocity <= 0
        voice = prepare_drum_voice(source, note, timestamp_us)
        write_driver(:sound, note, velocity, voice) unless voice.nil?
        return
      end
      allocator = @allocator
      voice_ids = voice_ids_for(source)
      return if @voice_pools && voice_ids.nil?
      old_voice = allocator.voice_for(channel, note, source: source)
      voice = allocator.allocate(
        channel,
        note,
        source: source,
        priority: priority,
        voice_ids: voice_ids
      )
      return if voice.nil?
      stolen = allocator.last_stolen
      if stolen && stolen[1] == PSG::DRUM_CHANNEL
        @driver.sound_stop
        if @drum_voice == voice
          @drum_voice = nil
          @drum_until_us = 0
        end
      end
      if stolen || old_voice
        write_driver(:mute, voice, 1, 0)
        if stolen
          @sustained.delete(note_key(stolen[0], stolen[1], stolen[2]))
        end
      end
      @velocities[voice] = velocity
      @sustained.delete(note_key(source, channel, note))
      state = state_for(source, channel)
      timbre = state[8]
      legato = state[9]
      lfo_depth = state[10]
      lfo_rate = state[11]
      mixer = state[12]
      write_period(voice, note, state)
      write_driver(:set_timbre, voice, timbre, 0)
      write_driver(:set_pan, voice, midi_pan(state), 0)
      write_driver(:set_legato, voice, legato, 0)
      write_driver(:set_lfo, voice, lfo_depth, lfo_rate, 0)
      apply_mixer(voice, mixer)
      write_driver(:send_reg, voice + 8, voice_volume(voice, state), 0)
      write_driver(:mute, voice, 0, 0)
    end

    private def note_off(source, channel, note)
      return if channel == PSG::DRUM_CHANNEL
      allocator = @allocator
      voice = allocator.voice_for(channel, note, source: source)
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
      allocator = @allocator
      voice_count = allocator.voice_count
      timbre = state[8]
      while i < voice_count
        entry = allocator.entry(i)
        write_driver(:set_timbre, i, timbre, 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def control_change(source, channel, controller, value)
      state = state_for(source, channel)
      case controller
      when 1
        state[10] = value
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
      when :env_enabled
        state[13] = event[3] != 0
        update_volumes(source, channel, state)
      when :lfo
        state[10] = event[3]
        state[11] = event[4]
        update_lfo(source, channel, state)
      when :lfo_rate
        state[11] = event[3]
        update_lfo(source, channel, state)
      when :bend_range
        state[7] = event[3]
        update_periods(source, channel, state)
      when :mixer
        state[12] = event[3]
        update_mixer(source, channel, state)
      when :noise
        write_driver(:send_reg, 6, event[3], 0)
      end
    end

    private def psg_global_event(event)
      case event[1]
      when :env_period
        value = event[2]
        write_driver(:send_reg, 11, value & 0xFF, 0)
        write_driver(:send_reg, 12, value >> 8, 0)
      when :env_shape
        write_driver(:send_reg, 13, event[2], 0)
      when :noise_period
        write_driver(:send_reg, 6, event[2], 0)
      end
    end

    private def state_for(source, channel)
      key = [source, channel]
      states = @states
      state = states[key]
      unless state
        state = [127, 127, 64, 8192, false, 127, 127,
                 DEFAULT_PITCH_BEND_RANGE, 0, 0, 0, 0, 0, false]
        states[key] = state
      end
      state
    end

    private def sustain_change(source, channel, state, enabled)
      was_enabled = state[4]
      state[4] = enabled
      return if enabled || !was_enabled
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      sustained = @sustained
      while i < voice_count
        entry = allocator.entry(i)
        if entry && owned_by?(entry, source, channel) && sustained[note_key(source, channel, entry[2])]
          release_voice(i, source, channel, entry[2])
        end
        i += 1
      end
    end

    private def all_notes_off(source, channel, immediate)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      state = state_for(source, channel)
      sustained = @sustained
      while i < voice_count
        entry = allocator.entry(i)
        if entry && owned_by?(entry, source, channel)
          if immediate || !state[4]
            release_voice(i, source, channel, entry[2])
          else
            sustained[note_key(source, channel, entry[2])] = true
          end
        end
        i += 1
      end
    end

    private def prepare_drum_voice(source, note, timestamp_us)
      duration = PSG.sound_duration(note)
      return nil if duration <= 0

      allocator = @allocator
      voice_ids = voice_ids_for(source)
      return nil if @voice_pools && voice_ids.nil?
      voice = nil
      drum_voice = @drum_voice
      if drum_voice
        entry = allocator.entry(drum_voice)
        if entry && entry[1] == PSG::DRUM_CHANNEL && voice_allowed?(drum_voice, voice_ids)
          voice = allocator.reserve(drum_voice, PSG::DRUM_CHANNEL, note, source: source, priority: DRUM_PRIORITY)
        else
          if entry && entry[1] == PSG::DRUM_CHANNEL
            allocator.release(entry[1], entry[2], source: entry[0])
            write_driver(:mute, drum_voice, 1, 0)
          end
          @drum_voice = nil
          @drum_until_us = 0
        end
      end
      if voice.nil?
        voice = allocator.allocate(
          PSG::DRUM_CHANNEL,
          note,
          source: source,
          priority: DRUM_PRIORITY,
          voice_ids: voice_ids
        )
      end
      return nil if voice.nil?

      entry = allocator.last_stolen
      @drum_voice = voice
      @velocities[voice] = 0
      @sustained.delete(note_key(entry[0], entry[1], entry[2])) if entry && entry[1] != PSG::DRUM_CHANNEL
      @drum_until_us = event_time_us(timestamp_us) + duration * 1000
      write_driver(:mute, voice, 1, 0)
      voice
    end

    private def release_expired_drum_voice(timestamp_us)
      drum_until_us = @drum_until_us
      return if drum_until_us <= 0
      return if event_time_us(timestamp_us) < drum_until_us

      voice = @drum_voice
      return if voice.nil?
      entry = @allocator.entry(voice)
      @allocator.release(entry[1], entry[2], source: entry[0]) if entry && entry[1] == PSG::DRUM_CHANNEL
      @drum_voice = nil
      @drum_until_us = 0
    end

    private def event_time_us(timestamp_us)
      return timestamp_us unless timestamp_us.nil?
      return Machine.uptime_us if defined?(Machine)
      0
    end

    private def release_voice(voice, source, channel, note)
      write_driver(:mute, voice, 1, 0)
      allocator = @allocator
      allocator.release(channel, note, source: source)
      @velocities[voice] = 0
      @sustained.delete(note_key(source, channel, note))
    end

    private def update_periods(source, channel, state)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      while i < voice_count
        entry = allocator.entry(i)
        write_period(i, entry[2], state) if entry && owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_volumes(source, channel, state)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      while i < voice_count
        entry = allocator.entry(i)
        write_driver(:send_reg, i + 8, voice_volume(i, state), 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_pan(source, channel, state)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      pan = midi_pan(state)
      while i < voice_count
        entry = allocator.entry(i)
        write_driver(:set_pan, i, pan, 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_legato(source, channel, state)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      legato = state[9]
      while i < voice_count
        entry = allocator.entry(i)
        write_driver(:set_legato, i, legato, 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_lfo(source, channel, state)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      depth = state[10]
      rate = state[11]
      while i < voice_count
        entry = allocator.entry(i)
        write_driver(:set_lfo, i, depth, rate, 0) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def update_mixer(source, channel, state)
      i = 0
      allocator = @allocator
      voice_count = allocator.voice_count
      mixer = state[12]
      while i < voice_count
        entry = allocator.entry(i)
        apply_mixer(i, mixer) if owned_by?(entry, source, channel)
        i += 1
      end
    end

    private def apply_mixer(voice, mode)
      mixer = @mixer
      case mode
      when 0
        mixer |= 1 << (voice + 3)
        mixer &= ~(1 << voice)
      when 1
        mixer &= ~(1 << (voice + 3))
        mixer |= 1 << voice
      when 2
        mixer &= ~(1 << voice)
        mixer &= ~(1 << (voice + 3))
      end
      @mixer = mixer
      write_driver(:send_reg, 7, mixer, 0)
    end

    private def voice_volume(voice, state)
      return 16 if state[13]
      volume = state[0]
      expression = state[1]
      velocities = @velocities
      velocity = velocities[voice]
      # @type var volume: Integer
      # @type var expression: Integer
      numerator = velocity * volume * expression * 15
      denominator = 127 * 127 * 127
      value = (numerator + denominator / 2) / denominator
      value = 1 if 0 < velocity && value == 0
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

    private def build_voice_pools(config, voice_count)
      return nil if config.nil?
      raise ArgumentError, "voice_pools must be a Hash" unless config.is_a?(Hash)

      pools = {} #: Hash[untyped, Array[Integer]]
      keys = config.keys
      key_count = keys.size
      key_index = 0
      next_voice = 0
      while key_index < key_count
        source = keys[key_index]
        value = config[source]
        ids = [] #: Array[Integer]
        if value.is_a?(Integer)
          unless 0 < value
            raise ArgumentError, "voice pool size must be a positive Integer"
          end
          if voice_count < next_voice + value
            raise ArgumentError, "voice pool sizes exceed available PSG voices"
          end
          i = 0
          while i < value
            ids << next_voice
            next_voice += 1
            i += 1
          end
        elsif value.is_a?(Array) && 0 < value.size
          i = 0
          value_size = value.size
          while i < value_size
            id = value[i]
            unless id.is_a?(Integer) && 0 <= id && id < voice_count && !ids.include?(id)
              raise ArgumentError, "voice pool IDs must be unique Integers in the available voice range"
            end
            ids << id
            i += 1
          end
        else
          raise ArgumentError, "voice pool must be a positive Integer or a non-empty Array"
        end
        pools[source] = ids
        key_index += 1
      end
      pools
    end

    private def voice_ids_for(source)
      pools = @voice_pools
      pools ? pools[source] : nil
    end

    private def voice_allowed?(voice, voice_ids)
      return true if voice_ids.nil?
      i = 0
      voice_ids_size = voice_ids.size
      while i < voice_ids_size
        return true if voice_ids[i] == voice
        i += 1
      end
      false
    end

    private def silence_all
      allocator = @allocator
      driver = @driver
      velocities = @velocities
      allocator.release_all
      @drum_voice = nil
      @drum_until_us = 0
      i = 0
      voice_count = allocator.voice_count
      while i < voice_count
        driver.mute_direct(i, 1)
        velocities[i] = 0
        i += 1
      end
      driver.buffer_flush
    end

    private def write_driver(command, arg1, arg2, arg3, arg4 = 0)
      driver = @driver
      while !@stopped
        pushed = case command
                 when :send_reg
                   driver.send_reg(arg1, arg2, arg3)
                 when :mute
                   driver.mute(arg1, arg2, arg3)
                 when :set_pan
                   driver.set_pan(arg1, arg2, arg3)
                 when :set_timbre
                   driver.set_timbre(arg1, arg2, arg3)
                 when :set_legato
                   driver.set_legato(arg1, arg2, arg3)
                 when :set_lfo
                   driver.set_lfo(arg1, arg2, arg3, arg4)
                 when :sound
                   driver.sound(arg1, arg2, arg3)
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
