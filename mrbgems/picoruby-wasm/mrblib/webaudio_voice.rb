module JS
  module WebAudio
    class Voice
      attr_reader :id, :analyser, :source, :channel, :note, :generation, :status

      def initialize(owner, context, id)
        @owner = owner
        @context = context
        @id = id
        @envelope = context.createGain
        @filter = context.createBiquadFilter
        @analyser = context.createAnalyser
        @analyser[:fftSize] = WebAudio::ANALYSER_FFT_SIZE
        @filter[:type] = "lowpass"
        @envelope.connect(@filter)
        @filter.connect(@analyser)
        @oscillator = nil
        @generation = 0
        @status = :idle
        @velocity_gain = 0.0
      end

      def start(source, channel, note, velocity)
        silence
        @generation += 1
        generation = @generation
        @source = source
        @channel = channel
        @note = note
        @status = :active
        @velocity_gain = velocity / 127.0

        context = @context
        now = context[:currentTime]
        tone = channel.tone
        @analyser.disconnect
        @analyser.connect(channel.destination)
        apply_filter(tone, now)

        oscillator = context.createOscillator
        @oscillator = oscillator
        oscillator[:type] = tone[:waveform]
        oscillator[:frequency].setValueAtTime(frequency_for(note), now)
        oscillator[:detune].setValueAtTime(tone[:detune] + channel.bend_cents, now)
        oscillator.connect(@envelope)

        gain = @envelope[:gain]
        gain.cancelScheduledValues(now)
        gain.setValueAtTime(0.0, now)
        attack_end = now + tone[:attack]
        decay_end = attack_end + tone[:decay]
        gain.linearRampToValueAtTime(@velocity_gain, attack_end)
        gain.linearRampToValueAtTime(@velocity_gain * tone[:sustain], decay_end)

        voice = self
        callback_id = nil
        callback_id = oscillator.addEventListener("ended") do |_event|
          JS::Object.removeEventListener(callback_id)
          voice.ended(generation)
        end
        oscillator.start(now)
        self
      end

      def release
        return self unless @status == :active
        @status = :releasing
        now = @context[:currentTime]
        release_time = @channel.tone[:release]
        gain = @envelope[:gain]
        current = gain[:value] || 0.0
        gain.cancelScheduledValues(now)
        gain.setValueAtTime(current, now)
        gain.linearRampToValueAtTime(0.0, now + release_time)
        @oscillator.stop(now + release_time + 0.005) if @oscillator
        self
      end

      def silence
        oscillator = @oscillator
        if oscillator
          now = @context[:currentTime]
          gain = @envelope[:gain]
          gain.cancelScheduledValues(now)
          gain.setValueAtTime(0.0, now)
          begin
            oscillator.stop(now)
          rescue
          end
        end
        @oscillator = nil
        @status = :idle
        self
      end

      def update_tone(changed_keys)
        return unless @status == :active || @status == :releasing
        channel = @channel
        tone = channel.tone
        now = @context[:currentTime]
        oscillator = @oscillator
        i = 0
        keys_size = changed_keys.size
        while i < keys_size
          key = changed_keys[i]
          case key
          when :waveform
            oscillator[:type] = tone[:waveform] if oscillator
          when :detune
            oscillator[:detune].setValueAtTime(tone[:detune] + channel.bend_cents, now) if oscillator
          when :cutoff, :resonance
            apply_filter(tone, now)
          when :sustain
            @envelope[:gain].setValueAtTime(@velocity_gain * tone[:sustain], now)
          end
          i += 1
        end
      end

      def update_pitch
        oscillator = @oscillator
        return unless oscillator
        tone = @channel.tone
        oscillator[:detune].setValueAtTime(
          tone[:detune] + @channel.bend_cents,
          @context[:currentTime]
        )
      end

      def ended(generation)
        return unless generation == @generation
        @oscillator = nil
        @status = :idle
        @owner.voice_ended(self, generation)
      end

      def snapshot
        {
          id: @id,
          source: @source,
          channel: @channel ? @channel.number : nil,
          note: @note,
          status: @status
        }
      end

      private def apply_filter(tone, now)
        @filter[:frequency].setValueAtTime(tone[:cutoff], now)
        @filter[:Q].setValueAtTime(tone[:resonance], now)
      end

      private def frequency_for(note)
        440.0 * (2.0 ** ((note - 69) / 12.0))
      end
    end
  end
end
