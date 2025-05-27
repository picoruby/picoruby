module PSG
  class Keyboard
    def self.note_freq(note)
      440.0 * (2.0 ** ((note - 69) / 12.0))
    end

    KEY2FREQ = {
       97 => note_freq(60), # 'a' => (C4)
      119 => note_freq(61), # 'w' => (C#4)
      115 => note_freq(62), # 's' => (D4)
      101 => note_freq(63), # 'e' => (D#4)
      100 => note_freq(64), # 'd' => (E4)
      102 => note_freq(65), # 'f' => (F4)
      116 => note_freq(66), # 't' => (F#4)
      103 => note_freq(67), # 'g' => (G4)
      121 => note_freq(68), # 'y' => (G#4)
      104 => note_freq(69), # 'h' => (A4)
      117 => note_freq(70), # 'u' => (A#4)
      106 => note_freq(71), # 'j' => (B4)
      107 => note_freq(72), # 'k' => (C5)
    }

    TONE_K = PSG::Driver::SAMPLE_RATE / 16.0 # clock factor
    CH = 2 # default channel

    def initialize(driver, channel: CH)
      @driver = driver
      @ch = channel
      @vol = 15
    end

    attr_accessor :vol

    def note_on(freq)
      period = (TONE_K / freq).to_i
      @driver.send_reg(@ch * 2,     period & 0xFF)
      @driver.send_reg(@ch * 2 + 1, period >> 8)
      @driver.send_reg(8 + @ch,     @vol)
    end

    def note_off
      @driver.send_reg(8 + @ch, 0)
    end

    def start
      prev_key = nil
      loop do
        key = STDIN.read_nonblock(1)&.ord
        if key.nil?
           note_off
          prev_key = nil
          next
        end
        if key != prev_key
          # previous key released
          # @type var prev_key: Integer
          note_off if prev_key && KEY2FREQ.has_key?(prev_key)
          # new key pressed
          if freq = KEY2FREQ[key]
            note_on(freq)
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
