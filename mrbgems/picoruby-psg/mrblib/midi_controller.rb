module PSG
  class MIDIController
    CC_TIMBRE = 20
    CC_ENV_PERIOD = 21
    CC_ENV_SHAPE = 22
    CC_ENV_ENABLED = 23
    CC_LFO_RATE = 24
    CC_MIXER = 25
    CC_NOISE_PERIOD = 26
    CC_BEND_RANGE = 27
    DEFAULT_SOURCE = :midi

    TIMBRE_NAMES = ["square", "triangle", "sawtooth", "inverse_sawtooth"]
    MIXER_NAMES = ["tone", "noise", "tone+noise"]

    def initialize(synth, logger: nil)
      unless synth.respond_to?(:handle)
        raise ArgumentError, "synth must respond to handle"
      end
      unless logger.nil? || logger.respond_to?(:puts)
        raise ArgumentError, "logger must respond to puts"
      end
      @synth = synth
      @fast_synth = synth.respond_to?(:handle_midi)
      @logger = logger
      @last_reported = {}
    end

    def handle(event, source: DEFAULT_SOURCE, priority: 0, timestamp_us: nil, **_context)
      handle_midi(event, source, priority, timestamp_us)
    end

    def handle_midi(event, source, priority, timestamp_us)
      command = event[0]
      if command == :control_change || command == :program_change
        mapped, report = map_event(event)
      else
        mapped = event
        report = nil
      end
      if @fast_synth
        accepted = @synth.handle_midi(mapped, source, priority, timestamp_us)
      else
        accepted = @synth.handle(
          mapped,
          source: source,
          priority: priority,
          timestamp_us: timestamp_us
        )
      end
      report_change(source, event, report) if accepted && report
      accepted
    end

    private def map_event(event)
      command = event[0]
      if command == :control_change
        map_control_change(event)
      elsif command == :program_change
        program = clamp(event[2], 0, 3)
        [event, [:channel, :timbre, TIMBRE_NAMES[program]]]
      else
        [event, nil]
      end
    end

    private def map_control_change(event)
      channel = event[1]
      controller = event[2]
      value = event[3]
      case controller
      when CC_TIMBRE
        timbre = clamp(value, 0, 3)
        [[:program_change, channel, timbre], [:channel, :timbre, TIMBRE_NAMES[timbre]]]
      when CC_ENV_PERIOD
        period = envelope_period(value)
        [[:psg_global, :env_period, period], [:global, :envelope_period, period]]
      when CC_ENV_SHAPE
        shape = clamp(value, 0, 15)
        [[:psg_global, :env_shape, shape], [:global, :envelope_shape, shape]]
      when CC_ENV_ENABLED
        enabled = 64 <= value ? 1 : 0
        [[:psg, channel, :env_enabled, enabled], [:channel, :envelope, enabled == 1 ? "on" : "off"]]
      when CC_LFO_RATE
        rate = (value * 255 + 63) / 127
        [[:psg, channel, :lfo_rate, rate], [:channel, :lfo_rate, "#{rate / 10}.#{rate % 10}Hz"]]
      when CC_MIXER
        mixer = clamp(value, 0, 2)
        [[:psg, channel, :mixer, mixer], [:channel, :mixer, MIXER_NAMES[mixer]]]
      when CC_NOISE_PERIOD
        period = clamp(value, 0, 31)
        [[:psg_global, :noise_period, period], [:global, :noise_period, period]]
      when CC_BEND_RANGE
        range = clamp(value, 0, 12)
        [[:psg, channel, :bend_range, range], [:channel, :bend_range, "#{range}st"]]
      else
        [event, standard_control_report(controller, value)]
      end
    end

    private def standard_control_report(controller, value)
      case controller
      when 1
        [:channel, :lfo_depth, "#{value}cent"]
      when 7
        [:channel, :level, value]
      when 10
        [:channel, :pan, (value * 15 + 63) / 127]
      when 11
        [:channel, :expression, value]
      when 64
        [:channel, :sustain, 64 <= value ? "on" : "off"]
      when 68
        [:channel, :legato, 64 <= value ? "on" : "off"]
      when 120
        [:channel, :panic, "all_sound_off"]
      else
        nil
      end
    end

    private def envelope_period(value)
      value = clamp(value, 0, 127)
      return 65_535 if value == 127
      value * value * 4
    end

    private def report_change(source, original_event, report)
      logger = @logger
      return unless logger

      scope = report[0]
      parameter = report[1]
      value = report[2]
      channel = original_event[1]
      report_channel = scope == :global ? nil : channel
      key = [source, scope, report_channel, parameter]
      # @type var key: PSG::MIDIController::report_key_t
      last_reported = @last_reported
      return if last_reported[key] == value
      last_reported[key] = value

      prefix = if scope == :global
                 "[PSG Global]"
               else
                 "[PSG #{source} ch#{channel + 1}]"
               end
      logger.puts "#{prefix} #{parameter}=#{value}"
    end

    private def clamp(value, min, max)
      return min if value < min
      return max if max < value
      value
    end
  end
end
