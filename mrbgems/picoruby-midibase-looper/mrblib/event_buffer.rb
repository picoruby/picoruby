module MIDIBASE
  class Looper
    class EventBuffer
      RECORD_SIZE = 5

      attr_reader :count, :max_events

      def initialize(max_events)
        unless max_events.is_a?(Integer) && 0 < max_events
          raise ArgumentError, "max_events must be a positive Integer"
        end
        @max_events = max_events
        @data = ""
        @count = 0
      end

      def append(tick, command, channel, note, velocity)
        return nil if @max_events <= @count
        validate_event(tick, command, channel, note, velocity)
        status = (command == :note_on ? MIDIBASE::NOTE_ON : MIDIBASE::NOTE_OFF) | channel
        data = @data
        offset = data.bytesize
        data << (tick & 0xFF).chr
        data << ((tick >> 8) & 0xFF).chr
        data << status.chr
        data << note.chr
        data << velocity.chr
        @count += 1
        offset
      end

      def tick_at(index)
        offset = checked_offset(index)
        (@data.getbyte(offset) || 0) | ((@data.getbyte(offset + 1) || 0) << 8)
      end

      def set_tick_at(index, tick)
        unless tick.is_a?(Integer) && 0 <= tick && tick <= 0xFFFF
          raise ArgumentError, "tick must be in 0..65535"
        end
        offset = checked_offset(index)
        @data.setbyte(offset, tick & 0xFF)
        @data.setbyte(offset + 1, (tick >> 8) & 0xFF)
        tick
      end

      def event_at(index)
        offset = checked_offset(index)
        status = @data.getbyte(offset + 2) || 0
        command = (status & 0xF0) == MIDIBASE::NOTE_ON ? :note_on : :note_off
        [
          command,
          status & 0x0F,
          @data.getbyte(offset + 3) || 0,
          @data.getbyte(offset + 4) || 0
        ]
      end

      def sort!
        i = 1
        count = @count
        while i < count
          j = i
          while 0 < j && compare(j, j - 1) < 0
            swap(j, j - 1)
            j -= 1
          end
          i += 1
        end
        self
      end

      def clear
        @data.clear
        @count = 0
        self
      end

      private def checked_offset(index)
        unless index.is_a?(Integer) && 0 <= index && index < @count
          raise IndexError, "event index out of range"
        end
        index * RECORD_SIZE
      end

      private def compare(left, right)
        left_tick = tick_at(left)
        right_tick = tick_at(right)
        return left_tick < right_tick ? -1 : 1 unless left_tick == right_tick

        left_status = @data.getbyte(left * RECORD_SIZE + 2) || 0
        right_status = @data.getbyte(right * RECORD_SIZE + 2) || 0
        left_off = (left_status & 0xF0) == MIDIBASE::NOTE_OFF
        right_off = (right_status & 0xF0) == MIDIBASE::NOTE_OFF
        return -1 if left_off && !right_off
        return 1 if !left_off && right_off
        0
      end

      private def swap(left, right)
        data = @data
        left_offset = left * RECORD_SIZE
        right_offset = right * RECORD_SIZE
        i = 0
        while i < RECORD_SIZE
          byte = data.getbyte(left_offset + i) || 0
          data.setbyte(left_offset + i, data.getbyte(right_offset + i) || 0)
          data.setbyte(right_offset + i, byte)
          i += 1
        end
      end

      private def validate_event(tick, command, channel, note, velocity)
        unless tick.is_a?(Integer) && 0 <= tick && tick <= 0xFFFF
          raise ArgumentError, "tick must be in 0..65535"
        end
        unless command == :note_on || command == :note_off
          raise ArgumentError, "only Note On/Off can be stored"
        end
        MIDIBASE.channel_value(channel)
        MIDIBASE.data_value(note)
        MIDIBASE.data_value(velocity)
      end
    end
  end
end
