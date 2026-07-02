require "midibase"

module USB
  module CDC
    class MIDIOutput
      DEFAULT_WRITE_TIMEOUT_MS = 100
      DEFAULT_SOURCE = :midi

      def initialize(write_timeout_ms: DEFAULT_WRITE_TIMEOUT_MS)
        unless write_timeout_ms.is_a?(Integer) && 0 <= write_timeout_ms
          raise ArgumentError, "write_timeout_ms must be a non-negative Integer"
        end
        @write_timeout_ms = write_timeout_ms
      end

      def connected?
        USB::CDC._midi_connected?
      end

      def putevent(command, *values)
        encoded = MIDIBASE.encode(command, *values)
        encoded_size = encoded.size
        bytes = "\0" * encoded_size
        i = 0
        while i < encoded_size
          bytes.setbyte(i, encoded[i])
          i += 1
        end
        write_bytes(bytes) ? encoded_size : false
      end

      def handle(event, **_context)
        handle_midi(event, DEFAULT_SOURCE, 0, nil)
      end

      def handle_midi(event, _source, _priority, _timestamp_us)
        command = event[0]
        unless command.is_a?(Symbol)
          raise ArgumentError, "MIDI event must start with a command Symbol"
        end
        values = []
        i = 1
        event_size = event.size
        while i < event_size
          values << event[i]
          i += 1
        end
        putevent(command, *values)
      end

      private def write_bytes(bytes)
        return false unless connected?

        offset = 0
        bytesize = bytes.bytesize
        deadline_us = Machine.uptime_us + @write_timeout_ms * 1_000
        while offset < bytesize
          written = USB::CDC._midi_write(bytes, offset)
          offset += written if 0 < written
          return false if offset < bytesize && deadline_us <= Machine.uptime_us
          if offset < bytesize
            Machine.tud_task
            sleep_ms 1
          end
        end
        true
      end
    end
  end
end
