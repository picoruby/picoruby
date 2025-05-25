module PSG
  class VGMParser
    VGM_HEADER_SIZE = 0x100
    CLOCK_PSG       = 3_579_545  # AY-3-8910 typical master clock

    VGM_WAIT_735 = 0x62
    VGM_WAIT_882 = 0x63
    VGM_WAIT_N   = 0x61
    VGM_DATA_END = 0x66

    VGM_CMD_SKIP_LENGTH = {
      0 => 0,  # no command
      0x4F => 1,  # GG stereo latch
      0x50 => 1,  # PSG DAC write
      0x67 => 7,  # data block header (we skip whole file for now)
      0x51 => 4,  # YM2413 etc. ┐
      0x52 => 4,  # YM2612       │  (unsupported chips)
      0x53 => 4   # YM2612 port1 ┘
    }

    def initialize(io)
      @io = io
      @buf = []
    end

    # Parse entire file into @buf : [{tick:, op:, reg:, val:}, ...]
    def parse
      @io.seek(0)
      hdr = @io.read(VGM_HEADER_SIZE)
      loop_tick = nil
      tick = 0

      until @io.eof?
        cmd = @io.getbyte
        case cmd
        when 0xA0          # PSG write:  addr, data
          reg  = @io.getbyte
          data = @io.getbyte
          @buf << {tick: tick, op: :reg, reg: reg || 0, val: data || 0}
        when VGM_WAIT_N    # wait n samples: 16-bit
          n  = ((@io.getbyte||0) << 8) | (@io.getbyte||0)  # little-endian to Integer
          tick += n
        when VGM_WAIT_735  # wait 735 ( NTSC 1/60 )
          tick += 735
        when VGM_WAIT_882  # wait 882 ( PAL  1/50 )
          tick += 882
        when VGM_DATA_END
          break
        else
          # Skip unsupported or DAC stream commands
          len = VGM_CMD_SKIP_LENGTH[cmd || 0]
          @io.read(len)
        end
      end
      self
    end

    # Provide iterator for sequencer
    def each_event
      if block_given?
        @buf.each { |ev| yield ev }
        self           # chainable
      else
        @buf           # plain Array, no Enumerator
      end
    end
  end
end
