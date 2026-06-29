module MIDIBASE
  class Looper
    class Recorder
      attr_reader :buffer, :voice_limit

      def initialize(loop_ticks:, grid_ticks:, voice_limit:, max_events:)
        @loop_ticks = loop_ticks
        @grid_ticks = grid_ticks
        @voice_limit = voice_limit
        @buffer = EventBuffer.new(max_events)
        @active = [] #: Array[Array[Integer]]
        @overflow = false
      end

      def note_on(tick, channel, note, velocity)
        existing = active_index(channel, note)
        note_off(tick, channel, note, 0) unless existing.nil?
        offset = @buffer.append(tick, :note_on, channel, note, velocity)
        if offset.nil?
          @overflow = true
          return false
        end
        @active << [channel, note, offset / EventBuffer::RECORD_SIZE, tick]
        true
      end

      def note_off(tick, channel, note, velocity)
        index = active_index(channel, note)
        return false if index.nil?
        active = @active.delete_at(index)
        on_index = active[2]
        raw_on = active[3]
        quantized_on, quantized_off = quantize_pair(raw_on, tick)
        @buffer.set_tick_at(on_index, quantized_on)
        offset = @buffer.append(quantized_off, :note_off, channel, note, velocity)
        if offset.nil?
          @overflow = true
          return false
        end
        true
      end

      def finish
        active = @active
        while 0 < active.size
          item = active[0]
          note_off(@loop_ticks, item[0], item[1], 0)
        end
        @buffer.sort!
        @overflow ? nil : @buffer
      end

      def overflow?
        @overflow
      end

      private def active_index(channel, note)
        i = 0
        active = @active
        active_size = active.size
        while i < active_size
          item = active[i]
          return i if item[0] == channel && item[1] == note
          i += 1
        end
        nil
      end

      private def quantize_pair(raw_on, raw_off)
        grid = @grid_ticks
        return [raw_on, raw_off] if grid.nil?
        snapped = ((raw_on + grid / 2) / grid) * grid
        delta = snapped - raw_on
        quantized_on = snapped
        quantized_off = raw_off + delta
        if @loop_ticks <= quantized_on
          quantized_on -= @loop_ticks
          quantized_off -= @loop_ticks
        end
        quantized_off = quantized_on + 1 if quantized_off <= quantized_on
        quantized_off = @loop_ticks if @loop_ticks < quantized_off
        [quantized_on, quantized_off]
      end
    end
  end
end
