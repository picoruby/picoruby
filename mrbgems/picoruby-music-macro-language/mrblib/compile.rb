class MML # Music Macro Language

  DURATION_BASE = (1000 * 60 * 4).to_f



  def initialize
    @octave = 4
    @tempo = 120
    @common_duration = (DURATION_BASE / @tempo / 4).to_i
    @q = 8   # # gate-time 1..8
    @volume = 15      # 0..15
    @transpose = 0    # in half-tone
    @chip_clock = 2_000_000
  end

  def compile(str)
    str.downcase!
    str = expand_loops(str)
    i = 0
    while c = str[i]
      case c.ord
      when 97..103 # 'a'..'g' # Note
        pitch, i = get_pitch(c, str[i + 1], i)
        length = case str[i + 1]
                 when "1".."9", "."
                 else
                   @common_duration
                 end
      when 114 # 'r' # Rest
      when 108 # 'l' # Length
      when 116 # 't' # Tempo
        tempo_str, i = number_str(str, i)
        @tempo = tempo_str.to_i unless tempo_str.empty?
        @common_duration = (DURATION_BASE / @tempo / 4).to_i
      when 111 # 'o' # Octave
        i += 1
        @octave = str[i].to_i
      when 113 # 'q' # Gate time
        i += 1
        @q = str[i].to_i
        @q = 8 if @q < 1 || 8 < @q
      when  60 # '>' # Octave down
        @octave -= 1
      when  62 # '<' # Octave up
        @octave += 1
      when 112 # 'p' # Pan
        num, i = number_str(str, i)
        n      = num.empty? ? 8 : num.to_i
        pan   = [[n, 0].max, 15].min
        yield(:pan, pan)
      when 118 # 'v' # Volume
        num, i = number_str(str, i)
        n      = num.empty? ? 15 : num.to_i
        volume = [[n, 0].max, 15].min
        yield(:volume, volume)
      when 115 # 's' # Envelope
        shape_str, i = number_str(str, i)
        unless shape_str.empty?
          env_shape = shape_str.to_i & 0x0F
          if str[i + 1] == ","
            i += 1
            period_str, i = number_str(str, i)
            env_period = period_str.to_i unless period_str.empty?
          end
          yield(:envelope, env_shape, env_period)
        end
      when 107 # 'k' # Tanpose (Key)
        sign = str[i + 1]
        if sign == "+" || sign == "-"
          num_str, i = number_str(str, i + 1)
          n = num_str.to_i
          @transpose = sign == "+" ? n : -n
        end
      when 109 # 'm' # LFO (Modulation)
        depth_str, i = number_str(str, i)
        unless depth_str.empty?
          mod_depth = depth_str.to_i * 100
          if str[i + 1] == ","
            i += 1
            rate_str, i = number_str(str, i)
            mod_rate = rate_str.to_i unless rate_str.empty?
          end
          yield(:modulation, mod_depth, mod_rate)
        end
      end
      i += 1
    end
  end

  # private

  NOTES = { a: 0, b: 2, c: 3, d: 5, e: 7, f: 8, g: 10 }

  def get_pitch(note, semitone, i)
    octave_fix = (note=='a'||note=='b') ? 1 : 0
    val = NOTES[note.to_sym]
    raise "Invalid note: #{note}" if val.nil?
    case semitone
    when '-'
      i += 1
      if note == 'a'
        octave_fix = 0
      else
        val -= 1
      end
    when '+'
      i += 1
      val += 1
    end
    pitch = 6.875 * (2<<(@octave + octave_fix)) * 2 ** (val / 12.0)
    pitch *= 2 ** (@transpose / 12.0)
    return [pitch, i + 1]
  end

  def number_str(str, i)
    str_number = ""
    i += 1
    while 47 < (c = (str[i]&.ord&.to_i).to_i) && c < 58
      str_number << str[i].to_s
      i += 1
    end
    return [str_number, i - 1]
  end

  def count_punto(str, i)
    punti = 0
    i += 1
    while str[i] == "."
      punti += 1
      i += 1
    end
    return [punti, i - 1]
  end

  def coef(punti)
    result = 1.0
    punti.times do |i|
      result += 0.5 ** (i + 1)
    end
    result
  end

  def expand_loops(str)
    expand(str, 0)[0]
  end

  def expand(str, i, depth = 0)
    raise "Loop nesting too deep" if depth > 8
    result = ""
    while i < str.size
      c = str[i]
      case c
      when "["
        i += 1
        inner, i = expand(str, i, depth + 1)
        count = 0
        while i < str.size
          o = str[i]&.ord || 0
          break unless 48 <= o && o <= 57  # '0'..'9'
          count = count * 10 + (o - 48)
          i += 1
        end
        result << inner * count
      when "]"
        return [result, i + 1]
      when "|"
        i += 1  # ignore
      else
        result << c.to_s
        i += 1
      end
    end
    [result, i]
  end

end
