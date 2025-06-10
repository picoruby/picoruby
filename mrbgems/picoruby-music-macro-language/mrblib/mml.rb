class MML # Music Macro Language

  DURATION_BASE = (1000 * 60 * 4).to_f
  NOTES = { a: 0, b: 2, c: 3, d: 5, e: 7, f: 8, g: 10 }

  def self.compile_multi(tracks)
    parsers = {}
    tracks.each do |ch, _track|
      parsers[ch] = MML.new
    end
    events   = []                                 # [start, ch, ...payload]
    total_ms = 0

    tracks.each do |ch, src|
      cursor = 0                                  # Time in the channel
      parsers[ch].compile(src) do |command, *args|
        events << [cursor, ch, command, *args]
        cursor += args[1].to_i if command == :play || command == :rest
      end
      total_ms = cursor if total_ms < cursor
    end

    # Equivalent to `events.sort_by! { |e| [e[0], e[1]] }`
    events.sort! do |a, b|
      if a[0] == b[0] # If cursor are eq, compare by ch
        a[1] <=> b[1]
      else
        a[0] <=> b[0]
      end
    end

    prev_time = 0

    events.each do |start, ch, command, *args|
      delta = start - prev_time
      yield(delta, ch, command, *args)
      prev_time = start
    end

    total_ms
  end

  def initialize
    @octave = 4
    @tempo = 120
    @q = 8   # # gate-time 1..8
    @volume = 8      # 0..15
    @transpose = 0    # in half-tone
    @detune = 0 # 0=no detune, 128=one octave down
    update_common_duration(4)
  end

  def compile(str)
    str = str.downcase
    str = expand_loops(str)
    total_duration = 0
    pos = 0
    while c = str[pos]
      pitch = nil
      case c.ord
      when  60 # '<' # Octave down
        @octave -= 1
      when  62 # '>' # Octave up
        @octave += 1
      when 97..103, 114 # 'a'..'g', 'r' # Note and Rest
        pitch, pos = get_pitch(c, str[pos + 1], pos)
        case str[pos + 1]
        when "1".."9", "."
          fraction_str, pos = number_str(str, pos - 1)
          punti, pos = count_punto(str, pos)
          if !fraction_str.empty?
            length = (DURATION_BASE / @tempo / fraction_str.to_i * coef(punti) + 0.5).to_i
          else
            length = (@common_duration * coef(punti) + 0.5).to_i
          end
        else
          length = @common_duration
        end
      when 105 # 'i' # Timbre
        pos += 1
        timbre = str[pos].to_i
        yield(:timbre, timbre)
      when 106 # 'j' # LFO (Modulation)
        depth_str, pos = number_str(str, pos)
        unless depth_str.empty?
          mod_depth = depth_str.to_i * 100
          mod_rate  = nil
          if str[pos + 1] == ","
            pos += 1
            rate_str, pos = number_str(str, pos)
            mod_rate = rate_str.to_i unless rate_str.empty?
          end
          yield(:lfo, mod_depth, mod_rate || 0)
        end
      when 107 # 'k' # Transpose (Key)
        sign = str[pos + 1]
        if sign == "+" || sign == "-"
          num_str, pos = number_str(str, pos + 1)
          n = num_str.to_i
          @transpose = sign == "+" ? n : -n
        end
      when 108 # 'l' # Length
        fraction_str, pos = number_str(str, pos)
        fraction = 0 < fraction_str.size ? fraction_str.to_i : nil
        if fraction
          punti, pos = count_punto(str, pos)
          update_common_duration(fraction * coef(punti))
        end
      when 111 # 'o' # Octave
        pos += 1
        @octave = str[pos].to_i
      when 112 # 'p' # Pan
        num, pos = number_str(str, pos)
        n      = num.empty? ? 8 : num.to_i
        pan   = [[n, 0].max, 15].min
        yield(:pan, pan || 8)
      when 113 # 'q' # Gate time
        pos += 1
        @q = str[pos].to_i
        @q = 8 if @q < 1 || 8 < @q
      when 115 # 's' # Envelope spape
        shape_str, pos = number_str(str, pos)
        yield(:volume, 16) # Use envelope instead of volume
        yield(:env_shape, shape_str.to_i & 0x0F)
      when 109 # 'm' # Envelope period
        period_str, pos = number_str(str, pos)
        yield(:env_period, period_str.to_i & 0xFFFF)
      when 116 # 't' # Tempo
        tempo_str, pos = number_str(str, pos)
        @tempo = tempo_str.to_i unless tempo_str.empty?
        update_common_duration(4) # Note: common fraction is also reset
      when 118 # 'v' # Volume
        num, pos = number_str(str, pos)
        n      = num.empty? ? 15 : num.to_i
        @volume = [[n, 0].max, 15].min
        yield(:volume, @volume)
      when 120 # 'x' # Mixer
        pos += 1
        yield(:mixer, str[pos].to_i)
      when 121 # 'y' # Noise period
        pos += 1
        yield(:noise, str[pos].to_i)
      when 122 # 'z' # Detune
        num, pos = number_str(str, pos)
        @detune = num.to_i
      else
        raise "Invalid character: #{c} (#{c.ord}) at position #{pos}"
      end

      pos += 1

      next if pitch.nil? || length.nil?
      sustain = if pitch == 0
                  0
                else
                  (@q == 8) ? length : (length / 8.0 * @q).to_i
                end
      release = length - sustain
      yield(:play, pitch, sustain) if 0 < sustain
      yield(:rest, 0,     release) if 0 < release
      total_duration += length
    end
    yield(:mute, 1)
    total_duration
  end

  # private

  def update_common_duration(fraction)
    @common_duration = (DURATION_BASE / @tempo / fraction + 0.5).to_i
  end

  def get_pitch(note, semitone, pos)
    return [0, pos + 1] if note == 'r' # Rest
    octave_fix = (note=='a'||note=='b') ? 1 : 0
    val = NOTES[note.to_sym]
    raise "Invalid note: #{note}" if val.nil?
    case semitone
    when '-'
      pos += 1
      if note == 'a'
        octave_fix = 0
      else
        val -= 1
      end
    when '+'
      pos += 1
      val += 1
    end
    pitch = 6.875 * (2<<(@octave + octave_fix)) * 2 ** (val / 12.0)
    pitch *= 2 ** (@transpose / 12.0) if @transpose != 0
    pitch /= (2 ** (@detune / 128.0)) if @detune != 0
    return [pitch, pos]
  end

  def number_str(str, pos)
    str_number = ""
    pos += 1
    while 47 < (c = (str[pos]&.ord&.to_i).to_i) && c < 58
      str_number << str[pos].to_s
      pos += 1
    end
    return [str_number, pos - 1]
  end

  def count_punto(str, pos)
    punti = 0
    pos += 1
    while str[pos] == "."
      punti += 1
      pos += 1
    end
    return [punti, pos - 1]
  end

  def coef(punti)
    result = 1.0
    punti.times do |pos|
      result += 0.5 ** (pos + 1)
    end
    result
  end

  def expand_loops(str)
    expand(str, 0)[0]
  end

  def expand(str, pos, depth = 0)
    raise "Loop nesting too deep" if depth > 8
    result = ""
    while pos < str.size
      c = str[pos]
      case c
      when "["
        pos += 1
        inner, pos = expand(str, pos, depth + 1)
        count = 0
        while pos < str.size
          o = str[pos]&.ord || 0
          break unless 48 <= o && o <= 57  # '0'..'9'
          count = count * 10 + (o - 48)
          pos += 1
        end
        result << inner * count
      when "]"
        return [result, pos + 1]
      when "|"
        pos += 1  # ignore
      else
        result << c.to_s
        pos += 1
      end
    end
    [result, pos]
  end

end
