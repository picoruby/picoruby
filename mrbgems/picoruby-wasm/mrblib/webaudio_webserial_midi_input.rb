module JS
  module WebAudio
    class WebSerialMIDIInput
      attr_reader :state, :error, :port, :gc_yield_event_count, :gc_yield_ms

      def initialize(router:, source: :webserial, on_state_change: nil, parser: nil,
                     gc_yield_event_count: 64, gc_yield_ms: 1)
        unless router.respond_to?(:emit_midi)
          raise ArgumentError, "router must respond to emit_midi"
        end
        unless on_state_change.nil? || on_state_change.respond_to?(:call)
          raise ArgumentError, "on_state_change must respond to call"
        end
        unless gc_yield_event_count.nil? || (gc_yield_event_count.is_a?(Integer) && 0 <= gc_yield_event_count)
          raise ArgumentError, "gc_yield_event_count must be nil or a non-negative Integer"
        end
        unless gc_yield_ms.is_a?(Integer) && 0 <= gc_yield_ms
          raise ArgumentError, "gc_yield_ms must be a non-negative Integer"
        end
        @router = router
        @source = source
        @on_state_change = on_state_change
        @parser = parser || MIDIBASE::Parser.new
        @gc_yield_event_count = gc_yield_event_count
        @gc_yield_ms = gc_yield_ms
        @state = :disconnected
        @error = nil
        @port = nil
      end

      def connect(baud_rate: 115_200)
        change_state(:connecting)
        input = self
        port = JS::WebSerial.connect(baud_rate: baud_rate)
        @port = port
        port.on_receive do |chunk|
          input.feed(chunk, timestamp_us: Machine.uptime_us)
        end
        port.on_disconnect do
          input.disconnected
        end
        change_state(:connected)
        self
      rescue => e
        @error = e
        change_state(:error)
        self
      end

      def disconnect
        port = @port
        port.close if port && port.opened?
        @port = nil
        @parser.reset
        change_state(:disconnected)
        self
      rescue => e
        @error = e
        change_state(:error)
        self
      end

      def feed(bytes, timestamp_us: Machine.uptime_us)
        unless bytes.is_a?(String)
          raise ArgumentError, "MIDI input must be a binary String"
        end
        parser = @parser
        router = @router
        source = @source
        count = 0
        yield_count = @gc_yield_event_count
        gc_yield_ms = @gc_yield_ms
        events_since_yield = 0
        i = 0
        bytesize = bytes.bytesize
        while i < bytesize
          event = parser.feed(bytes.getbyte(i))
          if event
            router.emit_midi(source, event, timestamp_us)
            count += 1
            if yield_count && 0 < yield_count && 0 < gc_yield_ms
              events_since_yield += 1
              if yield_count <= events_since_yield
                gc_yield(gc_yield_ms)
                events_since_yield = 0
              end
            end
          end
          i += 1
        end
        count
      end

      def stop
        disconnect
      end

      def disconnected
        @port = nil
        @parser.reset
        change_state(:disconnected)
        self
      end

      private def change_state(state)
        @state = state
        callback = @on_state_change
        callback.call(state, @error) if callback
      end

      private def gc_yield(ms)
        sleep_ms ms
      rescue
        nil
      end
    end
  end
end
