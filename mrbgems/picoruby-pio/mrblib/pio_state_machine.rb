module PIO
  class StateMachine
    def self.new(
      pio:,
      sm:,
      program:,
      freq: 125_000_000,
      out_pins: -1,
      out_pin_count: 1,
      set_pins: -1,
      set_pin_count: 1,
      in_pins: -1,
      sideset_pins: -1,
      jmp_pin: -1,
      out_shift_right: true,
      out_shift_autopull: false,
      out_shift_threshold: 32,
      in_shift_right: true,
      in_shift_autopush: false,
      in_shift_threshold: 32,
      fifo_join: PIO::FIFO_JOIN_NONE
    )
      self.init(
        pio,
        sm,
        program.instructions,
        program.length,
        program.side_set_count,
        program.side_set_optional ? 1 : 0,
        program.wrap_target_idx,
        program.wrap_idx,
        freq,
        out_pins,
        out_pin_count,
        set_pins,
        set_pin_count,
        in_pins,
        sideset_pins,
        jmp_pin,
        out_shift_right ? 1 : 0,
        out_shift_autopull ? 1 : 0,
        out_shift_threshold,
        in_shift_right ? 1 : 0,
        in_shift_autopush ? 1 : 0,
        in_shift_threshold,
        fifo_join
      )
    end

    def put_bytes(data)
      if data.is_a?(String)
        i = 0
        while i < data.length
          b = data.getbyte(i)
          put(b) if b
          i += 1
        end
      elsif data.is_a?(Array)
        i = 0
        while i < data.length
          put(data[i])
          i += 1
        end
      else
        raise ArgumentError, "data must be String or Array"
      end
    end

    def tx_full?
      0 < tx_full
    end

    def tx_empty?
      0 < tx_empty
    end

    def rx_full?
      0 < rx_full
    end

    def rx_empty?
      0 < rx_empty
    end
  end
end
