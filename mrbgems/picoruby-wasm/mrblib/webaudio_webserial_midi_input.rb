module JS
  module WebAudio
    class WebSerialMIDIInput
      attr_reader :state, :error, :port

      def initialize(router:, source: :webserial, on_state_change: nil, parser: nil)
        unless router.respond_to?(:emit_midi)
          raise ArgumentError, "router must respond to emit_midi"
        end
        unless on_state_change.nil? || on_state_change.respond_to?(:call)
          raise ArgumentError, "on_state_change must respond to call"
        end
        @router = router
        @source = source
        @on_state_change = on_state_change
        @parser = parser || MIDIBASE::Parser.new
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
        i = 0
        bytesize = bytes.bytesize
        while i < bytesize
          event = parser.feed(bytes.getbyte(i))
          if event
            router.emit_midi(source, event, timestamp_us)
            count += 1
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
    end
  end
end
