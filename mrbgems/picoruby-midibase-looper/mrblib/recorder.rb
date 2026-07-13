module MIDIBASE
  class Looper
    class Recorder
      attr_reader :buffer, :voice_limit, :channel_mask

      def initialize(loop_ticks:, grid_ticks:, voice_limit:, max_events:)
        @loop_ticks = loop_ticks
        @grid_ticks = grid_ticks
        @voice_limit = voice_limit
        @buffer = EventBuffer.new(max_events)
        @active = [] #: Array[Integer]
        @overflow = false
        @channel_mask = 0
      end

      def note_on(tick, channel, note, velocity)
        @channel_mask |= 1 << channel
        existing = active_index(channel, note)
        note_off(tick, channel, note, 0) unless existing.nil?
        offset = @buffer.append(tick, :note_on, channel, note, velocity)
        if offset.nil?
          @overflow = true
          return false
        end
        active = @active
        active << channel
        active << note
        active << offset / EventBuffer::RECORD_SIZE
        active << tick
        true
      end

      def note_off(tick, channel, note, velocity)
        index = active_index(channel, note)
        return false if index.nil?
        active = @active
        on_index = active[index + 2]
        raw_on = active[index + 3]
        active.delete_at(index)
        active.delete_at(index)
        active.delete_at(index)
        active.delete_at(index)
        grid = @grid_ticks
        if grid
          snapped = ((raw_on + grid / 2) / grid) * grid
          delta = snapped - raw_on
          quantized_on = snapped
          quantized_off = tick + delta
          loop_ticks = @loop_ticks
          if loop_ticks <= quantized_on
            quantized_on -= loop_ticks
            quantized_off -= loop_ticks
          end
          quantized_off = quantized_on + 1 if quantized_off <= quantized_on
          quantized_off = loop_ticks if loop_ticks < quantized_off
        else
          quantized_on = raw_on
          quantized_off = tick
        end
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
          note_off(@loop_ticks, active[0], active[1], 0)
        end
        @buffer.sort!
        return nil if @overflow
        @buffer.seal!
        @buffer
      end

      def overflow?
        @overflow
      end

      private def active_index(channel, note)
        i = 0
        active = @active
        active_size = active.size
        while i < active_size
          return i if active[i] == channel && active[i + 1] == note
          i += 4
        end
        nil
      end
    end
  end
end
