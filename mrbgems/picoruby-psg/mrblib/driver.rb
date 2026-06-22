module PSG
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
