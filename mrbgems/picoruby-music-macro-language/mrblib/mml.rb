# MML stands for "Music Macro Language"

# Example:
# tracks = {
#   0 => "t120 l8 v12 p0 cdef",
#   1 => "t120 l4 v10 p15 g>ceg<c",
#   2 => "t120 l2 v8  p8  a2.g2",
#   3 => "t120 l16 v15 p8  r r r"
# }
# len = MML.compile_multi(tracks) do |dt, ch, pitch, dur, pan, vol, es, ep|
#   sleep_ms dt
#   puts "#{dt}, #{ch}, #{pitch}, #{dur}, #{pan}, #{vol}, #{es}, #{ep}"
# end
# puts "total #{len} ms"

class MML

  def self.compile_multi(tracks, &block)
    events   = []                                 # [start, ch, ...payload]
    total_ms = 0

    tracks.each do |ch, src|
      cursor = 0                                  # Time in the channel
      mml    = MML.new
      mml.compile(src) do |pitch, dur, pan, vol, es, ep|
        events << [cursor, ch, pitch, dur, pan, vol, es, ep]
        cursor += dur
      end
      total_ms = cursor if cursor > total_ms
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
    events.each do |start, ch, pitch, dur, pan, vol, es, ep|
      delta = start - prev_time
      yield(delta, ch, pitch, dur, pan, vol, es, ep)
      prev_time = start
    end

    total_ms
  end

  DURATION_BASE = (1000 * 60 * 4).to_f

  def initialize
    @octave = 4
    @tempo = 120
    @common_duration = (DURATION_BASE / @tempo / 4).to_i
    @q = 8   # # gate-time 1..8
    @pan = 8 # (0=L)..(8=center)..(15=R)
    @volume = 15      # 0..15
    @env_shape = nil  # 0..15
    @env_period = nil # ms
  end

  def compile(str)
    i = 0
    str.each_char do |c|
      ord = c.ord
      if 64 < ord && ord < 91
        str[i] = (ord + 32).chr
      end
      i += 1
    end
    i = -1
    size = str.length
    lazy_sustain = 0
    total_duration = 0
    while true do
      i += 1
      break if i == size
      octave_fix = 0
      note = case str[i]
      when "r"; -1
      when "a"; octave_fix = 1; 0
      when "b"; octave_fix = 1; 2
      when "c"; 3
      when "d"; 5
      when "e"; 7
      when "f"; 8
      when "g"; 10
      else
        case str[i]
        when "t"
          tempo_str, i = number_str(str, i)
          @tempo = tempo_str.to_i if 0 < tempo_str.size
          @common_duration = (DURATION_BASE / @tempo / 4).to_i
        when "o"
          i += 1
          @octave = str[i].to_i
        when "q"
          i += 1
          @q = str[i].to_i
          @q = 8 if @q < 1 || 8 < @q
        when ">"
          @octave -= 1
        when "<"
          @octave += 1
        when "l"
          fraction_str, i = number_str(str, i)
          fraction = 0 < fraction_str.size ? fraction_str.to_i : nil
          if fraction
            punti, i = count_punto(str, i)
            @common_duration = (DURATION_BASE / @tempo / fraction * coef(punti)).to_i
          end
        when "p" # --------- new: PAN -----------
          num, i = number_str(str, i)
          n      = num.empty? ? 8 : num.to_i
          @pan   = [[n, 0].max, 15].min
        when "v"
          num, i = number_str(str, i)
          n      = num.empty? ? 15 : num.to_i
          @volume = [[n, 0].max, 15].min
        when "s"
          shape_str, i = number_str(str, i)
          if 0 < shape_str.size
            @env_shape = shape_str.to_i & 0x0F
            if str[i + 1] == ","
              i += 1
              period_str, i = number_str(str, i)
              @env_period = period_str.to_i if 0 < period_str.size
            end
          else
            @env_shape = nil
            @env_period = nil
          end
        end
        -2 # Neither a note nor a rest
      end
      pitch = if note == -2
        nil
      elsif note == -1
        0
      else
        case str[i + 1]
        when "-"
          if note == 0
            note = 11
            octave_fix = 0
          else
            note -= 1
          end
          i += 1
        when "+"
          note += 1
          i += 1
        end
        # @type var note: Integer
        6.875 * (2<<(@octave + octave_fix)) * 2 ** (note / 12.0)
      end
        fraction_str, i = number_str(str, i)
        punti, i = count_punto(str, i)
        duration = if 0 < fraction_str.size
          (DURATION_BASE / @tempo / fraction_str.to_i * coef(punti)).to_i
        else
          (@common_duration * coef(punti)).to_i
        end
      next unless pitch
      total_duration += duration
      sustain = (pitch == 0 || @q == 8) ? duration : (duration / 8.0 * @q).to_i
      release = duration - sustain
      if pitch == 0
        yield(0.0, lazy_sustain + sustain, @pan, @volume, @env_shape, @env_period)
        lazy_sustain = 0
      else
        yield(0.0, lazy_sustain, @pan, @volume, @env_shape, @env_period) if 0 < lazy_sustain
        yield(pitch.to_f, sustain, @pan, @volume, @env_shape, @env_period)
      end
      lazy_sustain = release
    end
    yield(0.0, lazy_sustain, @pan, @volume, @env_shape, @env_period) if 0 < lazy_sustain
    return total_duration
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

end
