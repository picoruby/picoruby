module JS
  module WebAudio
    class Channel
      attr_reader :source, :number, :program, :tone

      def initialize(context, destination, source, number)
        @context = context
        @source = source
        @number = number
        @gain_node = context.createGain
        @pan_node = context.createStereoPanner
        @gain_node.connect(@pan_node)
        @pan_node.connect(destination)
        reset
      end

      def reset
        @volume = 100
        @expression = 127
        @pan = 64
        @pitch_bend = 8192
        @program = 0
        @tone = WebAudio::DEFAULT_TONE.dup
        apply_gain
        apply_pan
        self
      end

      def destination
        @gain_node
      end

      def volume=(value)
        @volume = midi_value(value)
        apply_gain
      end

      def expression=(value)
        @expression = midi_value(value)
        apply_gain
      end

      def pan=(value)
        @pan = midi_value(value)
        apply_pan
      end

      def pitch_bend=(value)
        value = 0 if value < 0
        value = 16_383 if 16_383 < value
        @pitch_bend = value
      end

      def program=(value)
        presets = WebAudio::PRESETS
        @program = value
        @tone = presets[value % presets.size].dup
      end

      def update_tone(attributes)
        keys = attributes.keys
        i = 0
        keys_size = keys.size
        tone = @tone
        while i < keys_size
          key = keys[i]
          tone[key] = validate_tone_value(key, attributes[key])
          i += 1
        end
        self
      end

      def bend_cents
        delta = @pitch_bend - 8192
        divisor = delta < 0 ? 8192.0 : 8191.0
        delta * WebAudio::DEFAULT_PITCH_BEND_RANGE * 100.0 / divisor
      end

      def snapshot
        {
          source: @source,
          channel: @number,
          program: @program,
          volume: @volume,
          expression: @expression,
          pan: @pan,
          pitch_bend: @pitch_bend,
          tone: @tone.dup
        }
      end

      private def apply_gain
        value = (@volume / 127.0) * (@expression / 127.0)
        param = @gain_node[:gain]
        param.setValueAtTime(value, @context[:currentTime])
      end

      private def apply_pan
        value = if @pan < 64
                  (@pan - 64) / 64.0
                else
                  (@pan - 64) / 63.0
                end
        param = @pan_node[:pan]
        param.setValueAtTime(value, @context[:currentTime])
      end

      private def midi_value(value)
        value = 0 if value < 0
        value = 127 if 127 < value
        value
      end

      private def validate_tone_value(key, value)
        case key
        when :waveform
          unless WebAudio::WAVEFORMS.include?(value)
            raise ArgumentError, "waveform must be sine, square, triangle, or sawtooth"
          end
          value
        when :attack, :decay, :release
          validate_range(key, value, 0.0, 10.0)
        when :sustain
          validate_range(key, value, 0.0, 1.0)
        when :detune
          validate_range(key, value, -1_200.0, 1_200.0)
        when :cutoff
          sample_rate = @context[:sampleRate] || 44_100
          validate_range(key, value, 20.0, sample_rate / 2.0)
        when :resonance
          validate_range(key, value, 0.0, 30.0)
        else
          raise ArgumentError, "unknown tone parameter: #{key}"
        end
      end

      private def validate_range(key, value, min, max)
        unless value.is_a?(Integer) || value.is_a?(Float)
          raise ArgumentError, "#{key} must be Numeric"
        end
        number = value.to_f
        unless min <= number && number <= max
          raise ArgumentError, "#{key} must be in #{min}..#{max}"
        end
        number
      end
    end
  end
end
