module MIDIBASE
  class Looper
    class InputPump
      def initialize(input, output:, source:)
        unless input.respond_to?(:getevent)
          raise ArgumentError, "input must respond to getevent"
        end
        unless output.respond_to?(:emit)
          raise ArgumentError, "output must respond to emit"
        end
        @input = input
        @output = output
        @source = source
        @stopped = false
        @task = nil
      end

      def start
        pump = self
        @task = Task.new(name: "MIDIBASE::Looper::InputPump") { pump.run }
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
