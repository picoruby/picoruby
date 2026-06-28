module MIDIBASE
  class Parser
    def initialize(max_sysex_bytes: DEFAULT_MAX_SYSEX_BYTES)
      unless max_sysex_bytes.is_a?(Integer) && 0 < max_sysex_bytes
        raise ArgumentError, "max_sysex_bytes must be a positive Integer"
      end
      @max_sysex_bytes = max_sysex_bytes
      @running_status = nil
      @status = nil
      @expected = 0
      @data = []
      @sysex = nil
      @sysex_overflow = false
    end

    def reset
      @running_status = nil
      @status = nil
      @expected = 0
      @data.clear
      @sysex = nil
      @sysex_overflow = false
      self
    end

    def feed(byte)
      if byte < 0 || 0xFF < byte
        raise ArgumentError, "MIDI byte must be an Integer in 0..255"
      end

      if 0xF8 <= byte
        return realtime_event(byte)
      end

      if 0x80 <= byte
        return receive_status(byte)
      end

      if sysex = @sysex
        # @type var sysex: String
        if @sysex_overflow
          return nil
        elsif sysex.bytesize < @max_sysex_bytes
          sysex << byte
        else
          @sysex_overflow = true
        end
        return nil
      end

      if @status.nil?
        running_status = @running_status
        return nil if running_status.nil?
        begin_message(running_status)
      end

      data = @data
      data << byte
      return nil if data.size < @expected

      status = @status
      return nil if status.nil?
      event = build_event(status, data)
      if status < 0xF0
        begin_message(status)
      else
        @status = nil
        @expected = 0
        data.clear
      end
      event
    end

    private def receive_status(status)
      if @sysex && status != END_OF_EXCLUSIVE
        @sysex = nil
        @sysex_overflow = false
      end
      case status
      when SYSTEM_EXCLUSIVE
        @running_status = nil
        @status = nil
        @data.clear
        @sysex = ""
        @sysex_overflow = false
        nil
      when END_OF_EXCLUSIVE
        @running_status = nil
        @status = nil
        @data.clear
        sysex = @sysex
        return nil unless sysex

        event = if @sysex_overflow
                  [:system_exclusive_error, :too_large]
                else
                  [:system_exclusive, sysex]
                end
        @sysex = nil
        @sysex_overflow = false
        event
      when TUNE_REQUEST
        cancel_running_status
        [:tune_request]
      when 0xF1, 0xF2, 0xF3
        cancel_running_status
        begin_message(status)
        nil
      when 0xF4, 0xF5
        cancel_running_status
        nil
      else
        @sysex = nil
        @sysex_overflow = false
        @running_status = status
        begin_message(status)
        nil
      end
    end

    private def cancel_running_status
      @running_status = nil
      @status = nil
      @expected = 0
      @data.clear
    end

    private def begin_message(status)
      @status = status
      @data.clear
      high = status & 0xF0
      @expected = if high == PROGRAM_CHANGE || high == CHANNEL_PRESSURE ||
                      status == TIME_CODE_QUARTER_FRAME || status == SONG_SELECT
                    1
                  else
                    2
                  end
    end

    private def realtime_event(status)
      case status
      when TIMING_CLOCK   then [:timing_clock]
      when START          then [:start]
      when CONTINUE       then [:continue]
      when STOP           then [:stop]
      when ACTIVE_SENSING then [:active_sensing]
      when SYSTEM_RESET   then [:system_reset]
      else nil
      end
    end

    private def build_event(status, data)
      if status < 0xF0
        channel = status & 0x0F
        case status & 0xF0
        when NOTE_OFF
          [:note_off, channel, data[0], data[1]]
        when NOTE_ON
          if data[1] == 0
            [:note_off, channel, data[0], 0]
          else
            [:note_on, channel, data[0], data[1]]
          end
        when POLYPHONIC_KEY_PRESSURE
          [:polyphonic_key_pressure, channel, data[0], data[1]]
        when CONTROL_CHANGE
          [:control_change, channel, data[0], data[1]]
        when PROGRAM_CHANGE
          [:program_change, channel, data[0]]
        when CHANNEL_PRESSURE
          [:channel_pressure, channel, data[0]]
        when PITCH_BEND
          [:pitch_bend, channel, data[0] | (data[1] << 7)]
        end
      else
        case status
        when TIME_CODE_QUARTER_FRAME
          [:time_code_quarter_frame, data[0]]
        when SONG_POSITION
          [:song_position, data[0] | (data[1] << 7)]
        when SONG_SELECT
          [:song_select, data[0]]
        end
      end
    end
  end
end
