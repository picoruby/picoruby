module PSG
  class Driver
    def initialize(type, **opt)
      case type
      when :pwm
        if opt[:left].nil? || opt[:right].nil?
          raise ArgumentError, "Missing required options for PWM driver"
        end
        Driver.select_pwm(opt[:left], opt[:right])
      when :mcp4921, :mcp4922
        if opt[:copi].nil? || opt[:sck].nil? || opt[:cs].nil? || opt[:ldac].nil?
          raise ArgumentError, "Missing required options for MCP4922 driver"
        end
        dac = case type
              when :mcp4921
                1
              when :mcp4922
                2
              else
                -1
              end
        Driver.select_mcp492x(dac, opt[:copi], opt[:sck], opt[:cs], opt[:ldac])
      else
        raise ArgumentError, "Unsupported driver type: #{type}"
      end
    end

    WAIT_MS = 10 # ms

    def play_mml(tracks)
      mixer = 0b111000 # Noise all off, Tone all on
      chip_clock = PSG::Driver::CHIP_CLOCK
      MML.compile_multi(tracks) do |delta, ch, command, *args|
        case command
        when :mute
          invoke :mute, ch, args[0]
        when :play
          tone_period = (chip_clock / (32 * args[0])).to_i
          invoke :send_reg, ch * 2, tone_period
          invoke :send_reg, ch * 2 + 1, tone_period, delta
        when :rest
          invoke :send_reg, ch * 2, 0
          invoke :send_reg, ch * 2 + 1, 0, delta
        when :volume
          invoke :send_reg, ch + 8, args[0], delta
        when :env_period
          invoke :send_reg, 11, args[0] & 0xFF, delta
          invoke :send_reg, 12, args[0] >> 8, delta
        when :env_shape
          invoke :send_reg, 13, args[0], delta
        when :timbre
          invoke :set_timbre, ch, args[0]
        when :pan
          invoke :set_pan, ch, args[0]
        when :lfo
          invoke :set_lfo, ch, args[0], args[1]
        when :mixer
          case args[0]
          when 0 # Tone on, Noise off
            mixer |= (1 << (ch + 3))  # Set noise bit (off)
            mixer &= ~(1 << ch)       # Clear tone bit (on)
          when 1 # Tone off, Noise on
            mixer &= ~(1 << (ch + 3)) # Clear noise bit (on)
            mixer |= (1 << ch)        # Set tone bit (off)
          when 2 # Tone on, Noise on
            mixer &= ~(1 << ch)       # Clear tone bit (on)
            mixer &= ~(1 << (ch + 3)) # Clear noise bit (on)
          end
          invoke :send_reg, 7, mixer
        when :noise
          invoke :send_reg, 6, args[0], delta
        end
      end
    end

    # private

    def invoke(command, arg1, arg2, arg3 = 0)
      while true
        pushed = case command
        when :mute
          mute(arg1, arg2)
        when :send_reg
          send_reg(arg1, arg2, arg3)
        when :set_pan
          set_pan(arg1, arg2)
        when :set_timbre
          set_timbre(arg1, arg2)
        when :set_lfo
          set_lfo(arg1, arg2, arg3)
        else
          raise "Unknown command: #{command}"
        end
        return if pushed
        sleep_ms WAIT_MS
      end
    end
  end
end
