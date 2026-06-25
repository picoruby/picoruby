module MIDIBASE
  module MML
    class Player
      attr_reader :sequence, :clock

      def initialize(sequence, clock: InternalClock.new, output:)
        @sequence = sequence
        @clock = clock
        @output = output
        @tempo = DEFAULT_TEMPO
        @tick = 0
        @stopped = false
        @paused = false
        @rewind_requested = false
        @task = nil
      end

      def start
        player = self
        @task = Task.new(name: "MIDIBASE::MML::Player") { player.run }
        self
      end

      def join
        @task&.join
        self
      rescue RuntimeError
        Task.run
        self
      end

      def stop
        @stopped = true
        self
      end

      def pause
        @paused = true
        self
      end

      def resume
        @paused = false
        self
      end

      def rewind
        @rewind_requested = true
        self
      end

      def stopped?
        @stopped
      end

      def paused?
        @paused
      end

      def position
        sequence = @sequence
        time_signature = sequence.time_signature
        numerator = time_signature[0]
        denominator = time_signature[1]
        ticks_per_beat = sequence.ppqn * 4 / denominator
        ticks_per_bar = ticks_per_beat * numerator
        in_bar = @tick % ticks_per_bar
        [@tick / ticks_per_bar + 1, in_bar / ticks_per_beat + 1, in_bar % ticks_per_beat]
      end

      def run
        clock = @clock
        sequence = @sequence
        external_clock = clock.external?
        generation = external_clock ? clock.wait_until_running(self) : 0
        while !@stopped
          reset_playback
          generation = clock.generation if external_clock
          restart = false
          while !@stopped
            if @rewind_requested
              restart = true
              break
            end
            item = sequence.next_event
            break if item.nil?
            delta = item[0]
            event = item[1]
            target_tick = @tick + delta
            result = if external_clock
                       clock.wait_until(target_tick, self, generation)
                     else
                       clock.wait_ticks(delta, sequence.ppqn, @tempo, self)
                     end
            if result == :restart
              restart = true
              break
            end
            break if @stopped
            @tick = target_tick
            dispatch(event)
          end
          if !restart && external_clock && !@stopped
            next_generation = clock.wait_for_restart(self, generation)
            restart = next_generation != generation
          end
          break unless restart
        end
      end

      private def reset_playback
        sequence = @sequence
        sequence.reset
        @tempo = DEFAULT_TEMPO
        @tick = 0
        @rewind_requested = false
      end

      private def dispatch(event)
        if event[0] == :tempo
          @tempo = event[1]
        else
          @output.handle(event)
        end
      end
    end
  end
end
