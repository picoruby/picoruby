module PSG
  class Playback

    WAIT_RETRY_MS = 1
    WAIT_SLICE_MS = 10

    attr_reader :queue, :tracks
    attr_accessor :tempo_scale

    def self.__register(playback)
      registry = @__registry
      unless registry
        registry = {} #: Hash[Integer, PSG::Playback]
        @__registry = registry
      end

      next_id = @__next_id || 0
      next_id += 1
      @__next_id = next_id
      registry[next_id] = playback
      next_id
    end

    def self.__fetch(id)
      registry = @__registry
      return nil unless registry
      registry[id]
    end

    def self.__unregister(id)
      return unless id
      registry = @__registry
      registry.delete(id) if registry
    end

    def self.__run_task(id, kind)
      playback = __fetch(id)
      return unless playback

      if kind == :sound
        playback.__sound_loop
      else
        playback.__sequencer_loop
      end
    ensure
      playback.__task_finished if playback
    end

    def initialize(driver, tracks, loop: true, terminate: false)
      @driver = driver
      @tracks = tracks
      @loop = loop
      @terminate = terminate
      @queue = Task::Queue.new
      @tempo_scale = 1.0
      @stopped = false
      @paused = false
      @replay_requested = false
      @mixer = 0b111000
      @sequencer_task = nil
      @sound_task = nil
      @registry_id = nil
      @running_tasks = 0
    end

    def start
      @registry_id = self.class.__register(self)
      @running_tasks = 2
      @sound_task = __create_task(:sound)
      @sequencer_task = __create_task(:sequencer)
      self
    rescue
      self.class.__unregister(@registry_id) if @registry_id
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

    def __create_task(kind)
      id = @registry_id || raise("playback is not registered")
      if Task.respond_to?(:create)
        source = "PSG::Playback.__run_task(#{id}, :#{kind})"
        binary = PicoRubyVM::InstructionSequence.compile(source).to_binary
        task = Task.create(binary)
        task.run
        task
      else
        Task.new { PSG::Playback.__run_task(id, kind) }
      end
    end

    def __task_finished
      return unless @registry_id
      @running_tasks -= 1 if 0 < @running_tasks
      return unless @running_tasks == 0

      self.class.__unregister(@registry_id)
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

    def __sound_loop
      while true
        command = @queue.pop
        break if command.nil?
        __write_command(command)
      end
    rescue => e
      puts "Error during PSG sound task: #{e.message}"
      silence_direct
    end

    def __sequencer_loop
      while !@stopped
        @replay_requested = false
        @mixer = 0b111000
        @tracks.size.times { |tr| __enqueue([:mute, tr, 0]) }

        MML.compile_multi(@tracks, loop: @loop) do |delta, tr, command, arg0, arg1 = 0|
          break if @stopped || @replay_requested
          __wait_delta(delta)
          break if @stopped || @replay_requested
          __enqueue_mml_event(tr, command, arg0, arg1)
        end

        __enqueue_silence
        break unless @replay_requested
      end
    rescue => e
      puts "Error during PSG sequencer task: #{e.message}"
      silence_direct
    ensure
      @queue.close unless @queue.closed?
    end

    def __wait_delta(delta)
      remaining = delta.to_f
      while 0 < remaining && !@stopped && !@replay_requested
        while @paused && !@stopped && !@replay_requested
          sleep_ms WAIT_SLICE_MS
        end
        break if @stopped || @replay_requested

        scale = @tempo_scale || 1.0
        scale = 0.01 if scale <= 0
        score_slice = WAIT_SLICE_MS * scale
        if remaining < score_slice
          wall_ms = (remaining / scale).to_i
          wall_ms = 1 if wall_ms < 1
        else
          wall_ms = WAIT_SLICE_MS
        end
        sleep_ms wall_ms
        remaining -= wall_ms * scale
      end
    end

    def __enqueue_mml_event(tr, command, arg0, arg1)
      case command
      when :segno
        # MML handles the loop point internally.
      when :mute
        __enqueue([:mute, tr, arg0])
      when :play
        __enqueue([:send_reg, tr * 2, arg0 & 0xFF])
        __enqueue([:send_reg, tr * 2 + 1, (arg0 >> 8) & 0x0F])
      when :volume
        __enqueue([:send_reg, tr + 8, arg0])
      when :env_period
        __enqueue([:send_reg, 11, arg0 & 0xFF])
        __enqueue([:send_reg, 12, arg0 >> 8])
      when :env_shape
        __enqueue([:send_reg, 13, arg0])
      when :legato
        __enqueue([:set_legato, tr, arg0])
      when :timbre
        __enqueue([:set_timbre, tr, arg0])
      when :pan
        __enqueue([:set_pan, tr, arg0])
      when :lfo
        __enqueue([:set_lfo, tr, arg0, arg1])
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
        __enqueue([:send_reg, 7, @mixer])
      when :noise
        __enqueue([:send_reg, 6, arg0])
      end
    end

    def __enqueue_silence
      @tracks.size.times do |tr|
        __enqueue([:send_reg, tr + 8, 0])
        __enqueue([:mute, tr, 1])
      end
    end

    def __enqueue(command)
      return false if @stopped || @queue.closed?
      @queue.push(command)
      true
    rescue Task::Error
      false
    end

    def __write_command(command)
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

    def silence_direct
      tr = 0
      while tr < @tracks.size
        @driver.write_reg_direct(tr + 8, 0)
        @driver.mute_direct(tr, 1)
        tr += 1
      end
    end
  end

  class Driver

    WAIT_MS = 500

    def initialize(type, **opt)
      @mml_request = nil
      @mml_playback = nil
      @bgm_playback = nil
      case type
      when :pwm
        if opt[:left].nil? || opt[:right].nil?
          raise ArgumentError, "Missing required options for PWM driver"
        end
        Driver.select_pwm(opt[:left], opt[:right])
      when :mcp4922
        ldac = opt[:ldac]
        if ldac.nil?
          raise ArgumentError, "Missing required options for MCP4922 driver"
        end
        if opt[:cs] != ldac + 1 || opt[:sck] != ldac + 2 || opt[:copi] != ldac + 3
          raise ArgumentError, "Invalid pin configuration for MCP4922 driver"
        end
        Driver.select_mcp4922(ldac)
      else
        raise ArgumentError, "Unsupported driver type: #{type}"
      end
    end

    def join
      while true
        if buffer_empty?
          deinit
          break
        end
        sleep_ms WAIT_MS
      end
    end

    def replay
      @mml_playback&.replay
      @mml_request = :replay
    end

    def stop_mml
      @mml_playback&.stop
      @bgm_playback = nil if @bgm_playback == @mml_playback
      @mml_playback = nil
      @mml_request = :stop
    end

    def start_mml(tracks, loop: true, terminate: false)
      stop_mml
      trap
      @mml_request = nil
      @mml_playback = Playback.new(self, tracks, loop: loop, terminate: terminate)
      # @type ivar @mml_playback: Playback
      @mml_playback.start
    end

    def play_mml(tracks, terminate: true)
      playback = start_mml(tracks, loop: true, terminate: terminate)
      playback.join
      self
    rescue => e
      puts "Error during MML playback: #{e.message}"
      tracks.size.times { |tr| mute_direct(tr, 1) }
      deinit
      return self
    ensure
      @mml_playback = nil if @mml_playback == playback
    end

    def play_prs(filename, terminate: true)
      file = File.open(filename, "r")
      header = file.read(PRS::HEADER_SIZE)
      length, loop_start_pos = PRS.check_header(header.to_s)
      delta = 0
      while true
        op, val = file.read(2)&.bytes
        if op.nil? || val.nil?
          if 0 < loop_start_pos
            file.seek(loop_start_pos)
            tr = 0
            while tr < 3
              invoke :mute, tr, 0, 0
              tr += 1
            end
            next
          else
            join if terminate
            break
          end
        end
        case op & 0xF0
        when PRS::OP_WAIT
          val2, val3 = file.read(2)&.bytes
          break if val2.nil? || val3.nil?
          delta = val | (val2 << 8) | (val3 << 16)
          next
        when PRS::OP_MUTE
          invoke :mute, op & 0x0F, val, delta
        when PRS::OP_SEND_REG
          invoke :send_reg, op & 0x0F, val, delta
        when PRS::OP_SET_LEGATO
          invoke :set_legato, op & 0x0F, val, delta
        when PRS::OP_SET_PAN
          invoke :set_pan, op & 0x0F, val, delta
        when PRS::OP_SET_TIMBRE
          invoke :set_timbre, op & 0x0F, val, delta
        when PRS::OP_SET_LFO
          val2, val3 = file.read(2)&.bytes
          break if val2.nil? || val3.nil?
          invoke :set_lfo, val, val2, val3, delta
        end
        delta = 0
      end
    rescue => e
      puts "Error during MML playback: #{e.message}"
      3.times { |tr| invoke :mute, tr, 1, 0 }
      deinit
    ensure
      file&.close
    end

    # Start BGM playback in background PSG tasks.
    def start_bgm(tracks)
      stop_bgm
      @bgm_playback = start_mml(tracks, loop: true, terminate: false)
    end

    # Stop the BGM task and silence all channels.
    def stop_bgm
      return unless @bgm_playback
      # @type ivar @bgm_playback: Playback
      @bgm_playback.stop
      @mml_playback = nil if @mml_playback == @bgm_playback
      @bgm_playback = nil
      @mml_request = nil
    end

    # private

    def trap
      Signal.trap(:INT) do
        puts "Interrupt received, stopping playback..."
        # Immediately silence all channels (bypasses ring buffer, safe)
        write_reg_direct(8, 0)
        write_reg_direct(9, 0)
        write_reg_direct(10, 0)
        # Signal play_mml to exit cleanly.
        # Do NOT call deinit here: the BGM task is still alive and would
        # access the freed ring buffer, corrupting the heap.
        # play_mml will call join -> deinit when it gets scheduled.
        @mml_playback&.stop
        @mml_request = :quit
        Signal.trap(:INT, "DEFAULT")
      end
    end

    def invoke(command, arg1, arg2, arg3, arg4 = 0)
      while true
        return if @mml_request
        pushed = case command
        when :mute
          mute(arg1, arg2, arg3)
        when :send_reg
          send_reg(arg1, arg2, arg3)
        when :set_legato
          set_legato(arg1, arg2, arg3)
        when :set_pan
          set_pan(arg1, arg2, arg3)
        when :set_timbre
          set_timbre(arg1, arg2, arg3)
        when :set_lfo
          set_lfo(arg1, arg2, arg3, arg4)
        else
          raise "Unknown command: #{command}"
        end
        return if pushed
        GC.start
        if $DEBUG
          puts "Buffer full, retrying command: #{command} with args: #{arg1}, #{arg2}, #{arg3}, #{arg4}"
          p PicoRubyVM.memory_statistics
          p ObjectSpace.count_objects
        end
        sleep_ms WAIT_MS
      end
    end
  end
end
