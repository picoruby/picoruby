# Fork from picoruby-music-macro-language

class MML2MIDI

  def self.number_str(str, i)
    str_number = ""
    i += 1
    while 47 < (c = (str[i]&.ord&.to_i).to_i) && c < 58
      str_number << str[i].to_s
      i += 1
    end
    return [str_number, i - 1]
  end

  def self.count_punto(str, i)
    punti = 0
    i += 1
    while str[i] == "."
      punti += 1
      i += 1
    end
    return [punti, i - 1]
  end

  def self.coef(punti)
    result = 1.0
    punti.times do |i|
      result += 0.5 ** (i + 1)
    end
    result
  end

  def initialize
    @octave = 4
    @q = 8
    @common_duration = 4
  end

  def compile(str)
    data = {}
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
      when "c"; 12
      when "d"; 14
      when "e"; 16
      when "f"; 17
      when "g"; 19
      when "a"; 21
      when "b"; 23
      else
        case str[i]
        when "t"
          tempo_str, i = MML2MIDI.number_str(str, i)
          yield total_duration, [:tempo, tempo_str.to_i] if 0 < tempo_str.size
        when "h"
          ch_no_str, i = MML2MIDI.number_str(str, i)
          yield total_duration, [:ch, ch_no_str.to_i] if 0 < ch_no_str.size
        when "p"
          prg_no_str, i = MML2MIDI.number_str(str, i)
          yield total_duration, [:prg_no, prg_no_str.to_i] if 0 < prg_no_str.size
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
          fraction_str, i = MML2MIDI.number_str(str, i)
          fraction = 0 < fraction_str.size ? fraction_str.to_i : nil
          # FIXME: Syntax error when comment out
          # if fraction
          #   punti, i = MML2MIDI.count_punto(str, i)
          #   @common_duration = (fraction * MML2MIDI.coef(punti)).to_i
          # end
        end
        -2 # Neither a note nor a rest
      end
      fixed_note = if note == -2
        nil
      elsif note == -1
        0
      else
        case str[i + 1]
        when "-"
          if note == 0
            note = 11
            octave_fix = -1
          else
            note -= 1
          end
          i += 1
        when "+"
          note += 1
          i += 1
        end

        # @type var note: Integer
        (@octave + octave_fix) * 12 + note
      end
        fraction_str, i = MML2MIDI.number_str(str, i)
        punti, i = MML2MIDI.count_punto(str, i)
        duration = if 0 < fraction_str.size
          (16.0 / fraction_str.to_i * MML2MIDI.coef(punti)).to_i
        else
          (@common_duration * MML2MIDI.coef(punti)).to_i
        end
      next unless fixed_note
      sustain = (fixed_note == 0 || @q == 8) ? duration : (duration / 8.0 * @q).to_i
      release = duration - sustain
      if fixed_note == 0
        lazy_sustain = 0
      else
        yield total_duration, [:note, fixed_note, sustain]
      end
      lazy_sustain = release
      total_duration += duration
    end
    return total_duration
  end
end

