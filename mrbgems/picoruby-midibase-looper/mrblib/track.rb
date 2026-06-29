module MIDIBASE
  class Looper
    class Track
      attr_reader :events, :source, :voice_limit
      attr_accessor :muted

      def initialize(events, source:, voice_limit:)
        @events = events
        @source = source
        @voice_limit = voice_limit
        @muted = false
        @cursor = 0
        @cycle = 0
        @active = [] #: Array[Array[Integer]]
      end

      def reset
        @cursor = 0
        @cycle = 0
        @active.clear
        self
      end

      def next_tick(loop_ticks)
        return nil if @events.count == 0
        @cycle * loop_ticks + @events.tick_at(@cursor)
      end

      def current_event
        @events.event_at(@cursor)
      end

      def advance(loop_ticks)
        @cursor += 1
        if @events.count <= @cursor
          @cursor = 0
          @cycle += 1
        elsif @events.tick_at(@cursor) == 0 && 0 < @events.tick_at(@cursor - 1)
          @cycle += 1
        end
        loop_ticks
      end

      def seek(absolute_tick, loop_ticks)
        @active.clear
        cycle = absolute_tick / loop_ticks
        in_loop = absolute_tick % loop_ticks
        count = @events.count
        i = 0
        while i < count && @events.tick_at(i) < in_loop
          i += 1
        end
        if count <= i
          @cursor = 0
          @cycle = cycle + 1
        else
          @cursor = i
          @cycle = cycle
        end
        self
      end

      def note_started(event)
        @active << [event[1], event[2]]
      end

      def note_stopped(event)
        channel = event[1]
        note = event[2]
        i = 0
        active = @active
        active_size = active.size
        while i < active_size
          item = active[i]
          if item[0] == channel && item[1] == note
            active.delete_at(i)
            break
          end
          i += 1
        end
      end

      def take_active_notes
        active = @active
        @active = [] #: Array[Array[Integer]]
        active
      end
    end
  end
end
