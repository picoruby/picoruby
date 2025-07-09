module PSG
  class Driver

    WAIT_MS = 500

    def initialize(type, **opt)
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

    def play_mml(tracks, terminate: true)
      trap
      mixer = 0b111000 # Noise all off, Tone all on
      # Give the ring buffer time to fill with some amount of packets
      tracks.size.times { |tr| invoke :mute, tr, 0, WAIT_MS }
      MML.compile_multi(tracks, loop: true) do |delta, tr, command, arg0, arg1 = 0|
        case command
        when :segno
          # Ignore. MML takes care of `$` macro
        when :mute
          invoke :mute, tr, arg0, delta
        when :play
          invoke :send_reg, tr * 2    , arg0 & 0xFF       , delta
          invoke :send_reg, tr * 2 + 1, (arg0 >> 8) & 0x0F, 0
        when :volume
          invoke :send_reg, tr + 8, arg0, delta
        when :env_period
          invoke :send_reg, 11, arg0 & 0xFF, delta
          invoke :send_reg, 12, arg0 >> 8  , 0
        when :env_shape
          invoke :send_reg, 13, arg0, delta
        when :legato
          invoke :set_legato, tr, arg0, delta
        when :timbre
          invoke :set_timbre, tr, arg0, delta
        when :pan
          invoke :set_pan, tr, arg0, delta
        when :lfo
          invoke :set_lfo, tr, arg0, arg1, delta
        when :mixer
          case arg0
          when 0 # Tone on, Noise off
            mixer |= (1 << (tr + 3))  # Set noise bit (off)
            mixer &= ~(1 << tr)       # Clear tone bit (on)
          when 1 # Tone off, Noise on
            mixer &= ~(1 << (tr + 3)) # Clear noise bit (on)
            mixer |= (1 << tr)        # Set tone bit (off)
          when 2 # Tone on, Noise on
            mixer &= ~(1 << tr)       # Clear tone bit (on)
            mixer &= ~(1 << (tr + 3)) # Clear noise bit (on)
          end
          invoke :send_reg, 7, mixer, delta
        when :noise
          invoke :send_reg, 6, arg0, delta
        end
      end
      join if terminate
      return self
    rescue => e
      puts "Error during MML playback: #{e.message}"
      tracks.size.times { |tr| invoke :mute, tr, 1, 0 }
      deinit
      return self
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
            3.times do |tr|
              invoke :mute, tr, 0, 0
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

    # private

    def trap
      Signal.trap(:INT) do
        puts "Interrupt received, stopping playback..."
        deinit
        Signal.trap(:INT, "DEFAULT") # Reset the trap
      end
    end

    def invoke(command, arg1, arg2, arg3, arg4 = 0)
      while true
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
