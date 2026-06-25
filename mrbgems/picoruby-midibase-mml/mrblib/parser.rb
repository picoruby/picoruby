module MIDIBASE
  module MML
    class Parser
      NOTES = { 97 => 0, 98 => 2, 99 => 3, 100 => 5, 101 => 7, 102 => 8, 103 => 10 }
      DOT_NUMERATORS = [1, 3, 7, 15]
      DOT_DENOMINATORS = [1, 2, 4, 8]

      def initialize(track, channel:, ppqn: PPQN, loop: false, exception: true)
        @source = expand_loops(track.downcase)
        @channel = channel
        @ppqn = ppqn
        @loop = loop
        @raise_error = exception
        reset
      end

      def reset
        @cursor = 0
        @octave = 4
        @tempo = DEFAULT_TEMPO
        @gate = 8
        @transpose = 0
        @default_length = @ppqn
        @pending_ticks = 0
        @queue = [] #: Array[[Integer, Array[untyped]]]
        @queue_index = 0
        @finished = false
        @segno_pos = nil
        @bar = 1
        enqueue_rpn
        self
      end

      def next_event
        queued = shift_event
        return queued if queued
        return nil if @finished

        source = @source
        channel = @channel
        while true
          cursor = @cursor
          c = source.getbyte(cursor)
          if c.nil?
            if @loop && @segno_pos
              @cursor = @segno_pos
              next
            end
            @finished = true
            return nil
          end

          case c
          when 36 # $
            @segno_pos = cursor + 1
            push([:loop_point])
          when 60 # <
            @octave -= 1 if 1 < @octave
          when 62 # >
            @octave += 1 if @octave < 8
          when 64 # @
            value = read_number_after_cursor || 0
            push([:program_change, channel, clamp(value, 0, 127)])
          when 97..103 # a..g
            note = read_note(c, source.getbyte(@cursor + 1))
            length = read_length
            sustain = length * @gate / 8
            release = length - sustain
            push([:note_on, channel, note, 127])
            push([:note_off, channel, note, NOTE_OFF], sustain)
            @pending_ticks += release
          when 114 # r
            @pending_ticks += read_length
          when 106 # j
            depth = read_number_after_cursor || 0
            rate = 0
            if source.getbyte(@cursor + 1) == 44
              @cursor += 1
              rate = read_number_after_cursor || 0
            end
            push([:control_change, channel, 1, clamp(depth, 0, 127)])
            push([:psg, channel, :lfo, depth * 100, rate], 0)
          when 107 # k
            read_transpose
          when 108 # l
            fraction = read_number_after_cursor
            @default_length = length_ticks(fraction, count_dots) if fraction
          when 109 # m
            push([:psg, channel, :env_period, (read_number_after_cursor || 0) & 0xFFFF])
          when 111 # o
            value = read_number_after_cursor
            @octave = clamp(value || 4, 1, 8)
          when 112 # p
            value = clamp(read_number_after_cursor || 8, 0, 15)
            push([:control_change, channel, 10, scale_15_to_127(value)])
          when 113 # q
            value = read_number_after_cursor || 8
            @gate = clamp(value, 1, 8)
          when 115 # s
            push([:psg, channel, :env_shape, (read_number_after_cursor || 0) & 0x0F])
          when 116 # t
            value = read_number_after_cursor
            if value && 0 < value
              @tempo = value
              push([:tempo, value])
            end
          when 118 # v
            value = clamp(read_number_after_cursor || 15, 0, 15)
            push([:control_change, channel, 7, scale_15_to_127(value)])
          when 120 # x
            push([:psg, channel, :mixer, clamp(read_number_after_cursor || 0, 0, 2)])
          when 121 # y
            push([:psg, channel, :noise, clamp(read_number_after_cursor || 0, 0, 31)])
          when 122 # z
            detune = clamp(read_number_after_cursor || 0, 0, 128)
            push([:pitch_bend, channel, 8192 - detune * 8192 / 128])
          when 123 # {
            push([:control_change, channel, 68, 127])
          when 124 # |
            push([:barline, @bar])
            @bar += 1
          when 125 # }
            push([:control_change, channel, 68, 0])
          when 32, 9, 10, 13
            # whitespace
          else
            invalid_character(c)
          end

          @cursor += 1
          queued = shift_event
          return queued if queued
        end
      end

      private def enqueue_rpn
        channel = @channel
        push([:control_change, channel, 101, 0], 0)
        push([:control_change, channel, 100, 0], 0)
        push([:control_change, channel, 6, 12], 0)
        push([:control_change, channel, 38, 0], 0)
        push([:control_change, channel, 101, 127], 0)
        push([:control_change, channel, 100, 127], 0)
      end

      private def push(event, delta = nil)
        if delta.nil?
          delta = @pending_ticks
          @pending_ticks = 0
        end
        queue = @queue
        queue << [delta, event]
      end

      private def shift_event
        queue = @queue
        queue_index = @queue_index
        queue_size = queue.size
        return nil if queue_index >= queue_size
        event = queue[queue_index]
        queue_index += 1
        if queue_index >= queue_size
          queue.clear
          @queue_index = 0
        else
          @queue_index = queue_index
        end
        event
      end

      private def read_note(letter, accidental)
        value = NOTES[letter]
        raise "Invalid note" if value.nil?
        octave_fix = (letter == 97 || letter == 98) ? 1 : 0
        if accidental == 45
          @cursor += 1
          if letter == 97
            octave_fix = 0
            value += 11
          else
            value -= 1
          end
        elsif accidental == 43 || accidental == 35
          @cursor += 1
          value += 1
        end
        clamp(9 + 12 * (@octave + octave_fix) + value + @transpose, 0, 127)
      end

      private def read_length
        source = @source
        next_byte = source.getbyte(@cursor + 1)
        if next_byte && 48 <= next_byte && next_byte <= 57
          fraction = read_number_after_cursor
          length_ticks(fraction || 4, count_dots)
        elsif next_byte == 46
          length_ticks_from_base(@default_length, count_dots)
        else
          @default_length
        end
      end

      private def length_ticks(fraction, dots)
        base = @ppqn * 4
        length_ticks_from_base((base + fraction / 2) / fraction, dots)
      end

      private def length_ticks_from_base(base, dots)
        index = dots
        numerators = DOT_NUMERATORS
        denominators = DOT_DENOMINATORS
        numerators_size = numerators.size
        index = numerators_size - 1 if index >= numerators_size
        denominator = denominators[index]
        (base * numerators[index] + denominator / 2) / denominator
      end

      private def count_dots
        dots = 0
        source = @source
        cursor = @cursor
        while source.getbyte(cursor + 1) == 46
          cursor += 1
          dots += 1
        end
        @cursor = cursor
        dots
      end

      private def read_number_after_cursor
        source = @source
        i = @cursor + 1
        value = 0
        found = false
        while (c = source.getbyte(i)) && 48 <= c && c <= 57
          value = value * 10 + c - 48
          found = true
          i += 1
        end
        @cursor = i - 1 if found
        found ? value : nil
      end

      private def read_transpose
        sign = @source.getbyte(@cursor + 1)
        positive = sign == 43
        return unless positive || sign == 45
        @cursor += 1
        value = read_number_after_cursor || 0
        if positive
          @transpose = value
        else
          @transpose = -value
        end
      end

      private def scale_15_to_127(value)
        (value * 127 + 7) / 15
      end

      private def clamp(value, min, max)
        return min if value < min
        return max if max < value
        value
      end

      private def invalid_character(c)
        message = "Invalid MML character: `#{c.chr}` at position #{@cursor}"
        raise message if @raise_error
        puts "[WARN] #{message}"
      end

      private def expand_loops(source)
        source.include?("[") ? expand(source, 0, 0)[0] : source
      end

      private def expand(source, index, depth)
        raise "Loop nesting too deep" if 8 < depth
        result = ""
        while index < source.bytesize
          c = source.getbyte(index)
          if c == 91 # [
            inner, index = expand(source, index + 1, depth + 1)
            count = 0
            while (digit = source.getbyte(index)) && 48 <= digit && digit <= 57
              count = count * 10 + digit - 48
              index += 1
            end
            count = 1 if count == 0
            i = 0
            while i < count
              result << inner
              i += 1
            end
          elsif c == 93 # ]
            return [result, index + 1]
          else
            c ||= 0
            result << c
            index += 1
          end
        end
        [result, index]
      end
    end
  end
end
