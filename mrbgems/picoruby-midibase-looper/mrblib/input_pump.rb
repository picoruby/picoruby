module MIDIBASE
  class Looper
    class InputPump
      DEFAULT_PRIORITY = 64

      def initialize(input, output:, source:, priority: DEFAULT_PRIORITY)
        unless input.respond_to?(:getevent)
          raise ArgumentError, "input must respond to getevent"
        end
        unless output.respond_to?(:emit)
          raise ArgumentError, "output must respond to emit"
        end
        unless priority.is_a?(Integer) && 0 <= priority && priority <= 255
          raise ArgumentError, "priority must be in 0..255"
        end
        @input = input
        @output = output
        @source = source
        @priority = priority
        @stopped = false
        @task = nil
      end

      def start
        pump = self
        @task = Task.new(name: "MIDIBASE::Looper::InputPump", priority: @priority) { pump.run }
        self
      end

      def run
        input = @input
        output = @output
        source = @source
        while !@stopped
          event = input.getevent
          output.emit(source, event, timestamp_us: Machine.uptime_us)
        end
      end

      def stop
        @stopped = true
        @task&.terminate
        self
      end

      def join
        @task&.join
        self
      rescue RuntimeError
        Task.run
        self
      end
    end
  end
end
