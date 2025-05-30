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

      @psg_mixer = 0b11111111 # All channels are enabled by default
    end

    def play_note(ch, pitch, dur, pan, vol, es, ep)
      if pitch == 0
        send_reg(8 + ch, 0)
        return
      end

      period = hz_to_period(pitch)
      send_reg(0 + 2 * ch, period & 0xFF)
      send_reg(1 + 2 * ch, (period >> 8) & 0x0F)

      update_mixer_bit(ch, false)       # tone on
      update_mixer_bit(ch + 3, true)    # noise off
      send_reg(7, @psg_mixer)

      vol_reg = (es && ep) ? (0x10 | vol) : vol
      send_reg(8 + ch, vol_reg)
      set_pan(ch, pan)

      # Automaticaly set 0 (mute) after the duration
      send_reg(8 + ch, 0, dur)
    end

    # private

    def hz_to_period(hz)
      return 1 if hz <= 1
      raw = (CHIP_CLOCK / (32.0 * hz) + 0.5).to_i # +0.5 is workaround for #round
      raw < 1 ? 1 : raw > 4095 ? 4095 : raw
    end

    def update_mixer_bit(bit, enable)
      if enable
        @psg_mixer |= (1 << bit)   # invalidate
      else
        @psg_mixer &= ~(1 << bit)  # validate
      end
    end

  end
end
