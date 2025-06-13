module PSG
  class Keyboard
    def self.note_to_period(note)
      freq = 440.0 * (2.0 ** ((note - 69) / 12.0))
      (TONE_K / freq + 0.5).to_i
    end

    TONE_K = PSG::Driver::SAMPLE_RATE / 32.0 # clock factor
    KEY2PERIOD = {
       97 => note_to_period(60), # 'a' => (C4)
      119 => note_to_period(61), # 'w' => (C#4)
      115 => note_to_period(62), # 's' => (D4)
      101 => note_to_period(63), # 'e' => (D#4)
      100 => note_to_period(64), # 'd' => (E4)
      102 => note_to_period(65), # 'f' => (F4)
      116 => note_to_period(66), # 't' => (F#4)
      103 => note_to_period(67), # 'g' => (G4)
      121 => note_to_period(68), # 'y' => (G#4)
      104 => note_to_period(69), # 'h' => (A4)
      117 => note_to_period(70), # 'u' => (A#4)
      106 => note_to_period(71), # 'j' => (B4)
      107 => note_to_period(72), # 'k' => (C5)
    }
    CH = 2 # default channel

    def initialize(driver, channel: CH)
      @driver = driver
      @ch_reg = channel * 2
    end

    def note_on(period)
      @driver.send_reg(@ch_reg,     period & 0xFF, 0)
      @driver.send_reg(@ch_reg + 1, period >> 8, 0)
    end

    def note_off
      @driver.send_reg(@ch_reg,     0, 0)
      @driver.send_reg(@ch_reg + 1, 0, 0)
    end

    def start
      prev_key = nil
      while true
        key = STDIN.read_nonblock(1)&.ord
        if key.nil?
           note_off
          prev_key = nil
          next
        elsif key == 27 # ESC
          note_off
          break
        elsif key != prev_key
          # previous key released
          # @type var prev_key: Integer
          note_off if prev_key
          # new key pressed
          if period = KEY2PERIOD[key]
            note_on(period)
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
