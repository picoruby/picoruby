class MML # Music Macro Language

  DURATION_BASE = (1000 * 60 * 4).to_f
  NOTES = { a: 0, b: 2, c: 3, d: 5, e: 7, f: 8, g: 10 }

  def self.compile_multi(tracks, exception: true)
    parsers = {}
    tick_table = {}
    event_table = {}

    if tracks.empty?
      raise "No tracks provided for MML compilation"
    elsif 3 < tracks.size
      raise "Too many tracks for MML compilation: #{tracks.size} (maximum is 3)"
    end

    # Initialize parsers and get the first event for each track
    tracks.each_with_index do |track, track_id|
      parser = MML.new(track_id, track, exception: exception)
      parsers[track_id] = parser
      event = parser.reduce_next
      if event
        tick_table[track_id] = 0
        event_table[track_id] = event
      end
    end

    prev_time = 0

    while true
      # extract tracks with active events
      active_tracks = event_table.keys
      break if active_tracks.empty?

      # find the track with the earliest tick
      min_track = nil
      min_tick = nil
      event_table.keys.each do |tid|
        if min_tick.nil? || tick_table[tid] < min_tick
          min_tick = tick_table[tid]
          min_track = tid
        end
      end
      event = event_table[min_track]

      # @type var min_tick: Integer
      delta = min_tick - prev_time
      # @type var min_track: Integer
      yield(delta, min_track, *event)
      prev_time = min_tick

      next_event = parsers[min_track].reduce_next
      if next_event
        # add tick if play or rest
        if event[0] == :play || event[0] == :rest
          tick_table[min_track] += event[-1].to_i
        end
        event_table[min_track] = next_event
      else
        # delete if no more events
        tick_table.delete(min_track)
        event_table.delete(min_track)
      end
    end

    prev_time
  end

  def initialize(track_id, track, exception: true)
    @track_id = track_id
    @raise_err = exception
    @octave = 4
    @tempo = 120
    @q = 8          # gate-time 1..8
    @volume = nil   # 0..15
    @transpose = 0  # in half-tone
    @detune = 0     # 0=no detune, 128=one octave down
    @total_duration = 0
    @track = expand_loops(track)
    @cursor = 0
    update_common_duration(4)
    @event_queue = []
  end

  attr_reader :track_id

  def push_event(command, *args)
    @event_queue << [command, *args]
  end

  def reduce_next
    return @event_queue.shift unless @event_queue.empty?

    return nil if @finished

    while true
      c = @track[@cursor]
      if c.nil?
        push_event(:mute, 1) # Mute the track
        @finished = true
        break
      else
        c.downcase!
      end
      pitch = nil
      case c.ord
      when 36 # '$' # Segno = Loop start
        push_event(:segno)
      when 60 # '<' # Octave down
        @octave -= 1
      when 62 # '>' # Octave up
        @octave += 1
      when 97..103, 114 # 'a'..'g', 'r' # Note and Rest
        pitch = get_pitch(c, @track[@cursor + 1])
        case @track[@cursor + 1]
        when "1".."9", "."
          fraction = subvalue
          if fraction.nil?
            length = (@common_duration * coef(punti) + 0.5).to_i
          else
            length = (DURATION_BASE / @tempo / fraction * coef(punti) + 0.5).to_i
          end
        else
          length = @common_duration
        end
      when 105 # 'i' # Timbre
        @cursor += 1
        timbre = @track[@cursor].to_i
        push_event(:timbre, timbre)
      when 106 # 'j' # LFO (Modulation)
        depth = subvalue
        unless depth.nil?
          mod_depth = depth * 100
          if @track[@cursor + 1] == ","
            @cursor += 1
            mod_rate = subvalue
          end
          push_event(:lfo, mod_depth, mod_rate || 0)
        end
      when 107 # 'k' # Transpose (Key)
        sign = @track[@cursor + 1]
        if sign == "+" || sign == "-"
          @cursor += 1
          n = subvalue || 0
          @transpose = sign == "+" ? n : -n
        end
      when 108 # 'l' # Length
        fraction = subvalue
        if fraction
          update_common_duration(fraction * coef(punti))
        end
      when 111 # 'o' # Octave
        @cursor += 1
        @octave = @track[@cursor].to_i
      when 112 # 'p' # Pan
        pan = [subvalue, 15].min
        push_event(:pan, pan || 8)
      when 113 # 'q' # Gate time
        @cursor += 1
        @q = @track[@cursor].to_i
        @q = 8 if @q < 1 || 8 < @q
      when 115 # 's' # Envelope spape
        push_event(:volume, 16) # Use envelope instead of volume
        push_event(:env_shape, (subvalue || 0) & 0x0F)
      when 109 # 'm' # Envelope period
        push_event(:env_period, (subvalue || 0) & 0xFFFF)
      when 116 # 't' # Tempo
        @tempo = subvalue
        update_common_duration(4) # Note: common fraction is also reset
      when 118 # 'v' # Volume
        @volume = [subvalue, 15].min
        push_event(:volume, @volume || 15)
      when 120 # 'x' # Mixer
        @cursor += 1
        push_event(:mixer, @track[@cursor].to_i)
      when 121 # 'y' # Noise period
        @cursor += 1
        push_event(:noise, @track[@cursor].to_i)
      when 122 # 'z' # Detune
        @detune = subvalue
      when 32, 124 # ' ' or '|' to be ignored
        # skip
      else
        message = "TR: #{@track_id} Invalid character: #{c} (#{c.ord}) at position #{@cursor}"
        if @raise_err
          raise message
        else
          print "[WARN] "
          puts message
        end
      end

      @cursor += 1

      next if pitch.nil? || length.nil?
      sustain = if pitch == 0.0
                  0
                else
                  (@q == 8) ? length : (length / 8.0 * @q).to_i
                end
      release = length - sustain
      push_event(:play, pitch, sustain) if 0 < sustain
      push_event(:rest, 0,     release) if 0 < release
      @total_duration += length

      break unless @event_queue.empty?
    end
    return @event_queue.shift
  end

  # private

  def update_common_duration(fraction)
    @common_duration = (DURATION_BASE / @tempo / fraction + 0.5).to_i
  end

  def get_pitch(note, semitone)
    return 0.0 if note == 'r' # Rest
    octave_fix = (note=='a'||note=='b') ? 1 : 0
    val = NOTES[note.to_sym]
    raise "Invalid note: #{note}" if val.nil?
    case semitone&.ord
    when 45 # '-'
      @cursor += 1
      if note == 'a'
        octave_fix = 0
      else
        val -= 1
      end
    when 43, 35 # '+', '#'
      @cursor += 1
      val += 1
    end
    pitch = 6.875 * (2<<(@octave + octave_fix)) * 2 ** (val / 12.0)
    pitch *= 2 ** (@transpose / 12.0) if @transpose != 0
    pitch /= (2 ** (@detune / 128.0)) if @detune != 0
    return pitch
  end

  def subvalue
    str_number = ""
    @cursor += 1
    while 48 <= (c = @track[@cursor]&.ord || 0) && c <= 57
      str_number << @track[@cursor].to_s
      @cursor += 1
    end
    @cursor -= 1
    return str_number.empty? ? nil : str_number.to_i
  end

  def punti
    count = 0
    @cursor += 1
    while @track[@cursor] == "."
      count += 1
      @cursor += 1
    end
    @cursor -= 1
    return count
  end

  def coef(count)
    result = 1.0
    count.times do |index|
      result += 0.5 ** (index + 1)
    end
    result
  end

  def expand_loops(str)
    expand(str, 0)[0]
  end

  def expand(str, index, depth = 0)
    raise "Loop nesting too deep" if depth > 8
    result = ""
    while index < str.size
      c = str[index]
      case c
      when "["
        index += 1
        inner, index = expand(str, index, depth + 1)
        count = 0
        while index < str.size
          o = str[index]&.ord || 0
          break unless 48 <= o && o <= 57  # '0'..'9'
          count = count * 10 + (o - 48)
          index += 1
        end
        result << inner * count
      when "]"
        return [result, index + 1]
      when "|"
        index += 1  # ignore
      else
        result << c.to_s
        index += 1
      end
    end
    [result, index]
  end

end
