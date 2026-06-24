module PSG
  class Playback

    WAIT_RETRY_MS = 1
    WAIT_SLICE_MS = 10

    attr_reader :queue, :tracks, :external_bpm

    class << self
      def register(playback)
        @registry ||= {} #: Hash[Integer, PSG::Playback]
        @next_id ||= 0
        @registry[@next_id += 1] = playback
        @next_id
      end

      def unregister(id)
        id.nil? and return
        @registry.delete(id)
      end

      def run_task(id, kind)
        playback = @registry[id] or return

        if kind == :sound
          playback.sound_loop
        else
          playback.sequencer_loop
        end
      ensure
        playback&.task_finished
      end
    end

    def initialize(driver, tracks, loop: true, terminate: false)
      @driver = driver
      @tracks = tracks
      @loop = loop
      @terminate = terminate
      @queue = Task::Queue.new
      @tempo_scale_mille = 1000
      @external_bpm = nil
      @external_bpm_milli = nil
      @score_bpm = 120
      @stopped = false
      @paused = false
      @replay_requested = false
      @mixer = 0b111000
      @sequencer_task = nil
      @sound_task = nil
      @registry_id = nil
      @running_tasks = 0
    end

    def tempo_scale=(scale)
      raise ArgumentError, "tempo_scale must be positive" unless 0 < scale
      @tempo_scale_mille = (scale * 1000).to_i
      tempo_scale
    end

    def tempo_scale
      @tempo_scale_mille / 1000.0
    end

    def external_bpm=(bpm)
      if bpm.nil?
        @external_bpm = nil
        @external_bpm_milli = nil
      else
        raise ArgumentError, "external_bpm must be positive" unless 0 < bpm
        @external_bpm = bpm.to_f
        @external_bpm_milli = (bpm * 1000).to_i
      end
      @external_bpm
    end

    def start
      @registry_id = self.class.register(self)
      @running_tasks = 2
      @sound_task = create_task(:sound)
      @sequencer_task = create_task(:sequencer)
      self
    rescue
      self.class.unregister(@registry_id)
      @registry_id = nil
      @running_tasks = 0
      raise
    end

    def join
      begin
        @sequencer_task&.join
        @sound_task&.join
      rescue RuntimeError
        Task.run
      end
      @driver.join if @terminate
      self
    end

    private def create_task(kind)
      id = @registry_id || raise("playback is not registered")
      if Task.respond_to?(:create)
        source = "PSG::Playback.run_task(#{id}, :#{kind})"
        binary = PicoRubyVM::InstructionSequence.compile(source).to_binary
        task = Task.create(binary)
        task.run
        task
      else
        Task.new { PSG::Playback.run_task(id, kind) }
      end
    end

    def task_finished
      return unless @registry_id
      @running_tasks -= 1 if 0 < @running_tasks
      return unless @running_tasks == 0

      self.class.unregister(@registry_id)
      @registry_id = nil
    end

    def stop
      @stopped = true
      silence_direct
      @queue.clear unless @queue.closed?
      @queue.close unless @queue.closed?
      @driver.buffer_flush
      self
    end

    def pause
      @paused = true
      silence_direct
      @driver.buffer_flush
      self
    end

    def resume
      @paused = false
      self
    end

    def paused?
      @paused
    end

    def stopped?
      @stopped
    end

    def replay
      @replay_requested = true
      self
    end

    def sound_loop
      while true
        command = @queue.pop
        break if command.nil?
        write_command(command)
      end
    rescue => e
      puts "Error during PSG sound task: #{e.message}"
      silence_direct
    end

    def sequencer_loop
      while !@stopped
        @replay_requested = false
        @mixer = 0b111000
        @score_bpm = 120
        tr = 0
        while tr < @tracks.size
          enqueue([:mute, tr, 0])
          tr += 1
        end

        MML.compile_multi(@tracks, loop: @loop) do |delta_ms, tr, command, arg0, arg1 = 0|
          break if @stopped || @replay_requested
          wait_delta(delta_ms)
          break if @stopped || @replay_requested
          enqueue_mml_event(tr, command, arg0, arg1)
        end

        enqueue_silence
        break unless @replay_requested
      end
    rescue => e
      puts "Error during PSG sequencer task: #{e.message}"
      silence_direct
    ensure
      @queue.close unless @queue.closed?
    end

    private def wait_delta(delta_ms)
      remaining_us = delta_ms * 1000
      while 0 < remaining_us && !@stopped && !@replay_requested
        while @paused && !@stopped && !@replay_requested
          sleep_ms WAIT_SLICE_MS
        end
        break if @stopped || @replay_requested

        scale = effective_tempo_scale_mille
        score_slice_us = WAIT_SLICE_MS * scale
        if remaining_us < score_slice_us
          wall_ms = remaining_us / scale
          wall_ms = 1 if wall_ms < 1
          remaining_us = 0
        else
          wall_ms = WAIT_SLICE_MS
          remaining_us -= wall_ms * scale
        end
        sleep_ms wall_ms
      end
    end

    private def effective_tempo_scale_mille
      if @external_bpm_milli
        external_bpm_milli = @external_bpm_milli
        # @type var external_bpm_milli: Integer
        scale = external_bpm_milli / @score_bpm
        scale < 1 ? 1 : scale
      else
        @tempo_scale_mille
      end
    end

    private def enqueue_mml_event(tr, command, arg0, arg1)
      case command
      when :tempo
        @score_bpm = arg0
      when :segno
        # MML handles the loop point internally.
      when :mute
        enqueue([:mute, tr, arg0])
      when :play
        enqueue([:send_reg, tr * 2, arg0 & 0xFF])
        enqueue([:send_reg, tr * 2 + 1, (arg0 >> 8) & 0x0F])
      when :volume
        enqueue([:send_reg, tr + 8, arg0])
      when :env_period
        enqueue([:send_reg, 11, arg0 & 0xFF])
        enqueue([:send_reg, 12, arg0 >> 8])
      when :env_shape
        enqueue([:send_reg, 13, arg0])
      when :legato
        enqueue([:set_legato, tr, arg0])
      when :timbre
        enqueue([:set_timbre, tr, arg0])
      when :pan
        enqueue([:set_pan, tr, arg0])
      when :lfo
        enqueue([:set_lfo, tr, arg0, arg1])
      when :mixer
        case arg0
        when 0 # Tone on, Noise off
          @mixer |= (1 << (tr + 3))
          @mixer &= ~(1 << tr)
        when 1 # Tone off, Noise on
          @mixer &= ~(1 << (tr + 3))
          @mixer |= (1 << tr)
        when 2 # Tone on, Noise on
          @mixer &= ~(1 << tr)
          @mixer &= ~(1 << (tr + 3))
        end
        enqueue([:send_reg, 7, @mixer])
      when :noise
        enqueue([:send_reg, 6, arg0])
      end
    end

    private def enqueue_silence
      tr = 0
      while tr < @tracks.size
        enqueue([:send_reg, tr + 8, 0])
        enqueue([:mute, tr, 1])
        tr += 1
      end
    end

    private def enqueue(command)
      return false if @stopped || @queue.closed?
      @queue.push(command)
      true
    rescue Task::Error
      false
    end

    def write_command(command)
      while !@stopped
        pushed = case command[0]
        when :send_reg
          @driver.send_reg(command[1], command[2], 0)
        when :mute
          @driver.mute(command[1], command[2], 0)
        when :set_pan
          @driver.set_pan(command[1], command[2], 0)
        when :set_timbre
          @driver.set_timbre(command[1], command[2], 0)
        when :set_legato
          @driver.set_legato(command[1], command[2], 0)
        when :set_lfo
          @driver.set_lfo(command[1], command[2], command[3], 0)
        else
          true
        end
        return if pushed
        GC.start
        sleep_ms WAIT_RETRY_MS
      end
    end

    private def silence_direct
      tr = 0
      while tr < @tracks.size
        @driver.write_reg_direct(tr + 8, 0)
        @driver.mute_direct(tr, 1)
        tr += 1
      end
    end
  end
end
