require "midibase"

module PSG
  class Synth
    DEFAULT_PITCH_BEND_RANGE = 2
    DRUM_PRIORITY = 1_000_000
    PROGRAM_CHANNEL = 16
    WAIT_RETRY_MS = 1
    MIDI_MESSAGE_POOL_SIZE = 16
    DEFAULT_SOURCE = MIDIBASE::DEFAULT_SOURCE

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
      @program_cursors = Array.new(voice_count)
      @queue = Task::Queue.new
      @free_midi_messages = [] #: Array[PSG::Synth::midi_message_t]
      i = 0
      while i < MIDI_MESSAGE_POOL_SIZE
        @free_midi_messages << [nil, nil, 0, nil]
        i += 1
      end
      @mixer = 0b111000
      @noise_period = 0
      @stopped = false
      @task = nil
    end

    def start
      synth = self
      @task = Task.new(name: "PSG::Synth") { synth.run }
      self
    end

    def handle(event, source: DEFAULT_SOURCE, priority: 0, timestamp_us: nil, **_context)
      handle_midi(event, source, priority, timestamp_us)
    end

    def handle_midi(event, source, priority, timestamp_us)
      return false if @stopped

      queue = @queue
      return false if queue.closed?
      free_messages = @free_midi_messages
      message = free_messages.empty? ? [nil, nil, 0, nil] : free_messages.pop
      # @type var message: PSG::Synth::midi_message_t
      message[0] = event
      message[1] = source
      message[2] = priority
      message[3] = timestamp_us
      queue.push(message)
      true
    rescue Task::Error
      false
    end

    def trigger_program(name, velocity: 127, source: DEFAULT_SOURCE, priority: DRUM_PRIORITY, timestamp_us: nil)
      PSG.voice_program(name)
      handle([:voice_program, name, velocity], source: source, priority: priority, timestamp_us: timestamp_us)
    end

    def stop_program(name, source: DEFAULT_SOURCE, timestamp_us: nil)
      handle([:voice_program_off, name], source: source, timestamp_us: timestamp_us)
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
        now_us = current_time_us
        process_due_programs(now_us)
        envelope = pop_queue(queue, next_program_timeout_ms(now_us))
        if envelope.nil?
          break if queue.closed?
          next
        end
        process(envelope[0], envelope[1], envelope[2], envelope[3])
        envelope[0] = nil
        envelope[1] = nil
        envelope[2] = 0
        envelope[3] = nil
        @free_midi_messages << envelope
      end
    ensure
      silence_all
    end

    private def pop_queue(queue, timeout_ms)
      deadline = timeout_ms.nil? ? nil : queue.__deadline(timeout_ms)
      while true
        value = queue.__pop_try(false, deadline)
        return nil if value.equal?(Task::Queue::WAIT_TIMEOUT)
        return value unless value.equal?(Task::Queue::WAIT_RETRY)
      end
      nil
    end

    private def process(event, source, priority, timestamp_us)
      command = event[0]
      if command == :voice_program
        start_voice_program(source, PROGRAM_CHANNEL, event[1].object_id, PSG.voice_program(event[1]), event[2], priority)
        return
      elsif command == :voice_program_off
        stop_voice_program(source, PROGRAM_CHANNEL, event[1].object_id)
        return
      end
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
      process_due_programs(current_time_us)
    end

    private def note_on(source, channel, note, velocity, priority, _timestamp_us)
      if channel == PSG::DRUM_CHANNEL
        return if velocity <= 0
        program = PSG.drum_program(note)
        start_voice_program(source, channel, note, program, velocity, DRUM_PRIORITY) if program
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
      if stolen || old_voice
        write_driver(:mute, voice, 1, 0)
        cancel_program_cursor(voice)
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
      write_driver(:set_timbre, voice, timbre, 0)
      write_driver(:set_pan, voice, midi_pan(state), 0)
      write_driver(:set_legato, voice, legato, 0)
      write_driver(:set_lfo, voice, lfo_depth, lfo_rate, 0)
      write_voice(voice, period_for(note, state), @noise_period, voice_volume(voice, state), mixer_flags(mixer))
    end

    private def note_off(source, channel, note)
      if channel == PSG::DRUM_CHANNEL
        stop_voice_program(source, channel, note)
        return
      end
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
        @noise_period = event[3]
        write_driver(:send_reg, 6, @noise_period, 0)
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
        @noise_period = event[2]
        write_driver(:send_reg, 6, @noise_period, 0)
      end
    end

    private def state_for(source, channel)
      key = [source, channel]
      # @type var key: PSG::Synth::state_key_t
      states = @states
      state = states[key]
      unless state
        state = [127, 127, 64, 8192, false, 127, 127,
                 DEFAULT_PITCH_BEND_RANGE, 0, 0, 0, 0, 0, false]
        # @type var state: PSG::Synth::state_t
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

    private def start_voice_program(source, channel, note, program, velocity, priority)
      return if program.nil? || velocity <= 0
      velocity = 127 if 127 < velocity

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
      if stolen || old_voice
        write_driver(:mute, voice, 1, 0)
        cancel_program_cursor(voice)
        @sustained.delete(note_key(stolen[0], stolen[1], stolen[2])) if stolen
      end
      @velocities[voice] = velocity
      write_driver(:set_timbre, voice, 0, 0)
      write_driver(:set_pan, voice, 8, 0)
      write_driver(:set_legato, voice, 0, 0)
      write_driver(:set_lfo, voice, 0, 0, 0)
      now_us = current_time_us
      @program_cursors[voice] = [program, 0, now_us, velocity, source, channel, note]
      process_due_program_voice(voice, now_us)
    end

    private def stop_voice_program(source, channel, note)
      voice = @allocator.voice_for(channel, note, source: source)
      return if voice.nil? || @program_cursors[voice].nil?
      finish_voice_program(voice)
    end

    private def process_due_programs(now_us)
      i = 0
      cursors = @program_cursors
      cursor_count = cursors.size
      while i < cursor_count
        process_due_program_voice(i, now_us) if cursors[i]
        i += 1
      end
    end

    private def process_due_program_voice(voice, now_us)
      cursors = @program_cursors
      cursor = cursors[voice]
      while cursor && cursor[2] <= now_us
        program = cursor[0]
        index = cursor[1]
        if program.size <= index
          finish_voice_program(voice)
          return
        end
        step = program[index]
        volume = (step[2] * cursor[3] + 63) / 127
        flags = 0
        if 0 < volume
          flags |= 1 if 0 < step[0]
          flags |= 2 if 0 < step[1]
        end
        write_voice(voice, step[0], step[1], volume, flags)
        cursor[1] = index + 1
        cursor[2] += step[3] * 1000
        cursor = cursors[voice]
      end
    end

    private def next_program_timeout_ms(now_us)
      next_due_us = nil
      cursors = @program_cursors
      cursor_count = cursors.size
      i = 0
      while i < cursor_count
        cursor = cursors[i]
        if cursor && (next_due_us.nil? || cursor[2] < next_due_us)
          next_due_us = cursor[2]
        end
        i += 1
      end
      return nil if next_due_us.nil?
      # @type var next_due_us: Integer
      remaining_us = next_due_us - now_us
      return 0 if remaining_us <= 0
      (remaining_us + 999) / 1000
    end

    private def finish_voice_program(voice)
      cursor = @program_cursors[voice]
      return if cursor.nil?
      write_voice(voice, 0, 0, 0, 0)
      entry = @allocator.entry(voice)
      if entry && entry[0] == cursor[4] && entry[1] == cursor[5] && entry[2] == cursor[6]
        @allocator.release(entry[1], entry[2], source: entry[0])
      end
      @velocities[voice] = 0
      @program_cursors[voice] = nil
    end

    private def cancel_program_cursor(voice)
      @program_cursors[voice] = nil
    end

    private def current_time_us
      return Machine.uptime_us if defined?(Machine)
      Task.tick * 1000
    end

    private def release_voice(voice, source, channel, note)
      write_driver(:mute, voice, 1, 0)
      cancel_program_cursor(voice)
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
      mixer = update_mixer_bits(voice, mixer_flags(mode))
      write_driver(:send_reg, 7, mixer, 0)
    end

    private def mixer_flags(mode)
      case mode
      when 1
        2
      when 2
        3
      else
        1
      end
    end

    private def update_mixer_bits(voice, flags)
      mixer = @mixer
      if flags & 1 == 0
        mixer |= 1 << voice
      else
        mixer &= ~(1 << voice)
      end
      if flags & 2 == 0
        mixer |= 1 << (voice + 3)
      else
        mixer &= ~(1 << (voice + 3))
      end
      @mixer = mixer
      mixer
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
      period = period_for(note, state)
      write_driver(:send_reg, voice * 2, period & 0xFF, 0)
      write_driver(:send_reg, voice * 2 + 1, period >> 8 & 0x0F, 0)
    end

    private def period_for(note, state)
      bend_range = state[7]
      semitones = (state[3] - 8192) * bend_range.to_f / 8192.0
      PSG.note_to_period(note + semitones)
    end

    private def write_voice(voice, tone_period, noise_period, volume, flags)
      update_mixer_bits(voice, flags)
      write_driver(:voice_write, voice, tone_period, noise_period, volume, flags)
    end

    private def owned_by?(entry, source, channel)
      !entry.nil? && entry[0] == source && entry[1] == channel
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

    private def silence_all
      allocator = @allocator
      driver = @driver
      velocities = @velocities
      cursors = @program_cursors
      allocator.release_all
      i = 0
      voice_count = allocator.voice_count
      while i < voice_count
        driver.mute_direct(i, 1)
        velocities[i] = 0
        cursors[i] = nil
        i += 1
      end
      driver.buffer_flush
    end

    private def write_driver(command, arg1, arg2, arg3, arg4 = 0, arg5 = 0)
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
                 when :voice_write
                   driver.voice_write(arg1, arg2, arg3, arg4, arg5)
                 else
                   raise ArgumentError, "unknown PSG command: #{command}"
                 end
        return true if pushed
        sleep_ms WAIT_RETRY_MS
      end
      false
    end
  end
end
