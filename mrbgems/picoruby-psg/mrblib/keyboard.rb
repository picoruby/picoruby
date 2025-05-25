module PSG
  module Keyboard
    KEY2NOTE = {
      'a' => 60, 'w' => 61, 's' => 62, 'e' => 63, 'd' => 64,
      'f' => 65, 't' => 66, 'g' => 67, 'y' => 68, 'h' => 69,
      'u' => 70, 'j' => 71, 'k' => 72
    }

    TONE_K = 3_579_545.0 / 16.0   # AY clock factor
    CH     = 2                    # use channel C

    def self.note_freq(note)
      440.0 * (2.0 ** ((note - 69) / 12.0))
    end

    def self.note_on(note, vol = 15)
      period = (TONE_K / note_freq(note)).to_i
      PSG.send_reg(CH * 2,     period & 0xFF)
      PSG.send_reg(CH * 2 + 1, period >> 8)
      PSG.send_reg(8 + CH,     vol)
    end

    def self.note_off
      PSG.send_reg(8 + CH, 0)
    end

    def self.start
      prev_key = nil
      loop do
        key = STDIN.read_nonblock(1)
        if key.nil?
          if prev_key
            note_off
            prev_key = nil
          end
          next
        end
        if key != prev_key
          # previous key released
          # @type var prev_key: Integer
          note_off if prev_key && KEY2NOTE.has_key?(prev_key.chr)
          # new key pressed
          if note = KEY2NOTE[key]
            note_on(note)
            prev_key = key
          else
            prev_key = nil
          end
        end
        sleep_ms 1   # 1 ms poll; latency ≈ 2 ms
      end
    end

  end
end
