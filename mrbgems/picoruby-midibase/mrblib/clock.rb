module MIDIBASE
  class Clock
    CLOCKS_PER_QUARTER_NOTE = 24

    attr_reader :bpm, :time_signature

    def initialize(time_signature: [4, 4])
      @bpm = nil
      @running = false
      @pulse = 0
      @sample_started_at = nil
      @sample_intervals = 0
      self.time_signature = time_signature
    end

    def time_signature=(signature)
      unless signature.is_a?(Array) && signature.size == 2
        raise ArgumentError, "time_signature must be [numerator, denominator]"
      end
      numerator = signature[0]
      denominator = signature[1]
      unless numerator.is_a?(Integer) && 0 < numerator &&
             denominator.is_a?(Integer) && 0 < denominator &&
             (denominator & (denominator - 1)) == 0 &&
             denominator <= 32
        raise ArgumentError, "invalid time signature"
      end
      @time_signature = [numerator, denominator]
      @clocks_per_beat = 96 / denominator
      @pulse = 0
      @time_signature
    end

    def observe(event, timestamp_us = nil)
      case event[0]
      when :timing_clock
        observe_timing_clock(timestamp_us || Machine.uptime_us)
      when :start
        @pulse = 0
        @running = true
        restart_tempo_sample
      when :continue
        @running = true
        restart_tempo_sample
      when :stop
        @running = false
      when :system_reset
        reset
      end
      event
    end

    def reset
      @bpm = nil
      @running = false
      @pulse = 0
      @sample_started_at = nil
      @sample_intervals = 0
      self
    end

    def clock_running?
      @running
    end

    def position
      clocks_per_bar = @time_signature[0] * @clocks_per_beat
      in_bar = @pulse % clocks_per_bar
      [@pulse / clocks_per_bar + 1, in_bar / @clocks_per_beat + 1, in_bar % @clocks_per_beat]
    end

    def bar
      position[0]
    end

    def beat
      position[1]
    end

    def tick
      position[2]
    end

    private def observe_timing_clock(now_us)
      if @sample_started_at.nil?
        @sample_started_at = now_us
        @sample_intervals = 0
      else
        @sample_intervals += 1
        if CLOCKS_PER_QUARTER_NOTE <= @sample_intervals
          sample_started_at = @sample_started_at
          # @type var sample_started_at: Integer
          elapsed = now_us - sample_started_at
          @bpm = 60_000_000.0 / elapsed if 0 < elapsed
          @sample_started_at = now_us
          @sample_intervals = 0
        end
      end
      @pulse += 1 if @running
    end

    private def restart_tempo_sample
      @sample_started_at = nil
      @sample_intervals = 0
    end
  end
end
