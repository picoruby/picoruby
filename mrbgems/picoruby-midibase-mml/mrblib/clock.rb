require "midibase"

module MIDIBASE
  module MML
    PPQN = 480
    DEFAULT_TEMPO = 120
    NOTE_OFF = 0

    class InternalClock
      WAIT_SLICE_MS = 10

      def external?
        false
      end

      def wait_ticks(delta_ticks, ppqn, tempo, player)
        remaining_us = delta_ticks * 60_000_000 / (ppqn * tempo)
        while 0 < remaining_us && !player.stopped?
          while player.paused? && !player.stopped?
            sleep_ms WAIT_SLICE_MS
          end
          break if player.stopped?
          slice_us = WAIT_SLICE_MS * 1000
          slice_us = remaining_us if remaining_us < slice_us
          sleep_ms((slice_us + 999) / 1000)
          remaining_us -= slice_us
        end
        :ok
      end
    end

    class MIDIClock
      TICKS_PER_CLOCK = PPQN / 24

      def initialize
        @running = false
        @generation = 0
        @confirmed_tick = 0
        @last_pulse_us = nil
        @interval_us = nil
      end

      def external?
        true
      end

      def generation
        @generation
      end

      def handle(event, timestamp_us: nil, **_context)
        timestamp_us ||= Machine.uptime_us
        case event[0]
        when :start
          @generation += 1
          @confirmed_tick = 0
          @last_pulse_us = nil
          @interval_us = nil
          @running = true
        when :continue
          @last_pulse_us = nil
          @running = true
        when :stop
          @running = false
        when :system_reset
          @generation += 1
          @confirmed_tick = 0
          @last_pulse_us = nil
          @interval_us = nil
          @running = false
        when :timing_clock
          observe_pulse(timestamp_us) if @running
        end
        self
      end

      def wait_until(target_tick, player, generation)
        while !player.stopped?
          return :restart if generation != @generation
          if @running && target_tick <= estimated_tick
            return :ok
          end
          sleep_ms 1
        end
        :stopped
      end

      def wait_until_running(player)
        while !player.stopped? && !@running
          sleep_ms 1
        end
        @generation
      end

      def wait_for_restart(player, generation)
        while !player.stopped? && generation == @generation
          sleep_ms 1
        end
        @generation
      end

      private def observe_pulse(timestamp_us)
        if last = @last_pulse_us
          interval = timestamp_us - last
          if 0 < interval
            current_interval = @interval_us
            @interval_us = current_interval ? (current_interval * 3 + interval) / 4 : interval
          end
        end
        @last_pulse_us = timestamp_us
        @confirmed_tick += TICKS_PER_CLOCK
      end

      private def estimated_tick
        interval = @interval_us
        last = @last_pulse_us
        return @confirmed_tick unless interval && last && 0 < interval
        elapsed = Machine.uptime_us - last
        elapsed = 0 if elapsed < 0
        elapsed = interval if interval < elapsed
        @confirmed_tick + elapsed * TICKS_PER_CLOCK / interval
      end
    end
  end
end
