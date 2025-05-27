module PSG

  class VGMCompiler
    def initialize(mml)
      @mml = mml
    end

    def compile(tracks)
      vgm = "\x00" * 0x100
      tick = 0

      sample_rate = PSG::Driver::SAMPLE_RATE

      @mml.compile_multi(tracks) do |delta, ch, pitch, dur, pan, vol, es, ep|
        tick += (delta * sample_rate / 1000.0 + 0.5).to_i # +0.5 is workaround of round

        if pitch.is_a?(Symbol)
          # skip if :lfo, :lfo_off, :env
        else
          # @type var dur: Integer
          # @type var pan: Integer
          # @type var vol: Integer
          # @type var es: Integer | nil
          # @type var ep: Integer | nil
          regs = yield(ch, pitch, dur, pan, vol, es, ep)
          regs.each do |reg, val|
            vgm << 0xA0.chr << (reg & 0xFF).chr << (val & 0xFF).chr
          end
        end

        if tick >= 0xFFFF
          vgm << 0x61.chr << (tick & 0xFF).chr << ((tick >> 8) & 0xFF).chr
          tick = 0
        end
      end

      if tick > 0
        vgm << 0x61.chr << (tick & 0xFF).chr << ((tick >> 8) & 0xFF).chr
      end

      vgm << 0x66.chr
      vgm
    end
  end

end
