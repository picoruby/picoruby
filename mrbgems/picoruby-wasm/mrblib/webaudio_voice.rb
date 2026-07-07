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
        @sources = []
        @percussion = false
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
        @percussion = channel.percussion?

        context = @context
        now = context[:currentTime]
        tone = channel.tone
        @analyser.disconnect
        @analyser.connect(channel.destination)
        if @percussion
          start_percussion(note, now, generation)
          return self
        end

        now += WebAudio::NOTE_START_DELAY
        apply_filter(tone, now)

        oscillator = context.createOscillator
        @oscillator = oscillator
        @sources << oscillator
        oscillator[:type] = tone[:waveform]
        oscillator[:frequency].setValueAtTime(frequency_for(note), now)
        oscillator[:detune].setValueAtTime(tone[:detune] + channel.bend_cents, now)
        oscillator.connect(@envelope)

        gain = @envelope[:gain]
        gain.cancelScheduledValues(now)
        gain.setValueAtTime(0.0, now)
        attack = tone[:attack]
        attack = WebAudio::MIN_ATTACK if attack < WebAudio::MIN_ATTACK
        attack_end = now + attack
        decay_end = attack_end + tone[:decay]
        gain.linearRampToValueAtTime(@velocity_gain, attack_end)
        gain.linearRampToValueAtTime(@velocity_gain * tone[:sustain], decay_end)

        voice = self
        callback_id = oscillator.addEventListener("ended") do |_event|
          JS::Object.removeEventListener(callback_id)
          voice.ended(generation)
        end
        oscillator.start(now)
        self
      end

      def release
        return self unless @status == :active
        return self if @percussion
        @status = :releasing
        now = @context[:currentTime]
        channel = @channel
        return self unless channel
        release_time = channel.tone[:release]
        gain = @envelope[:gain]
        current = gain[:value] || 0.0
        gain.cancelScheduledValues(now)
        gain.setValueAtTime(current, now)
        gain.linearRampToValueAtTime(0.0, now + release_time)
        @oscillator.stop(now + release_time + 0.005) if @oscillator
        self
      end

      def silence
        sources = @sources
        if 0 < sources.size
          now = @context[:currentTime]
          gain = @envelope[:gain]
          gain.cancelScheduledValues(now)
          gain.setValueAtTime(0.0, now)
          i = 0
          sources_size = sources.size
          while i < sources_size
            begin
              sources[i].stop(now)
            rescue
            end
            i += 1
          end
        end
        @sources = []
        @oscillator = nil
        @status = :idle
        self
      end

      def update_tone(changed_keys)
        return if @percussion
        return unless @status == :active || @status == :releasing
        channel = @channel
        return unless channel
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
        return if @percussion
        oscillator = @oscillator
        return unless oscillator
        channel = @channel
        return unless channel
        tone = channel.tone
        oscillator[:detune].setValueAtTime(
          tone[:detune] + channel.bend_cents,
          @context[:currentTime]
        )
      end

      def ended(generation)
        return unless generation == @generation
        @oscillator = nil
        @sources = []
        @status = :idle
        @owner.voice_ended(self, generation)
      end

      def snapshot
        channel = @channel
        {
          id: @id,
          source: @source,
          channel: channel ? channel.number : nil,
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

      private def start_percussion(note, now, generation)
        case WebAudio::PERCUSSION_NOTES[note]
        when :kick
          start_kick(now, generation)
        when :snare
          start_snare(now, generation)
        when :closed_hat
          start_hat(now, generation, 0.075)
        when :open_hat
          start_hat(now, generation, 0.5)
        when :low_tom
          start_tom(now, generation, 120.0, 78.0, 0.34)
        when :mid_tom
          start_tom(now, generation, 165.0, 110.0, 0.28)
        when :high_tom
          start_tom(now, generation, 220.0, 150.0, 0.22)
        end
      end

      private def start_kick(now, generation)
        oscillator = @context.createOscillator
        oscillator[:type] = "sine"
        frequency = oscillator[:frequency]
        frequency.setValueAtTime(155.0, now)
        frequency.exponentialRampToValueAtTime(46.0, now + 0.12)
        oscillator.connect(@envelope)
        schedule_percussion_envelope(now, 0.48, 0.002)
        start_one_shot(oscillator, now, 0.48, generation)
      end

      private def start_snare(now, generation)
        context = @context
        noise = context.createBufferSource
        noise[:buffer] = @owner.noise_buffer
        noise_filter = context.createBiquadFilter
        noise_filter[:type] = "highpass"
        noise_filter[:frequency].setValueAtTime(1_100.0, now)
        noise.connect(noise_filter)
        noise_filter.connect(@envelope)

        oscillator = context.createOscillator
        oscillator[:type] = "triangle"
        oscillator[:frequency].setValueAtTime(180.0, now)
        tone_gain = context.createGain
        tone_gain[:gain].setValueAtTime(0.28, now)
        oscillator.connect(tone_gain)
        tone_gain.connect(@envelope)
        @sources << oscillator
        oscillator.start(now)
        oscillator.stop(now + 0.16)

        schedule_percussion_envelope(now, 0.2, 0.001)
        start_one_shot(noise, now, 0.2, generation)
      end

      private def start_hat(now, generation, duration)
        context = @context
        noise = context.createBufferSource
        noise[:buffer] = @owner.noise_buffer
        highpass = context.createBiquadFilter
        highpass[:type] = "highpass"
        highpass[:frequency].setValueAtTime(7_000.0, now)
        noise.connect(highpass)
        highpass.connect(@envelope)
        schedule_percussion_envelope(now, duration, 0.001)
        start_one_shot(noise, now, duration, generation)
      end

      private def start_tom(now, generation, start_frequency, end_frequency, duration)
        oscillator = @context.createOscillator
        oscillator[:type] = "triangle"
        frequency = oscillator[:frequency]
        frequency.setValueAtTime(start_frequency, now)
        frequency.exponentialRampToValueAtTime(end_frequency, now + duration * 0.7)
        oscillator.connect(@envelope)
        schedule_percussion_envelope(now, duration, 0.002)
        start_one_shot(oscillator, now, duration, generation)
      end

      private def schedule_percussion_envelope(now, duration, attack)
        @filter[:frequency].setValueAtTime(20_000.0, now)
        @filter[:Q].setValueAtTime(0.0, now)
        gain = @envelope[:gain]
        gain.cancelScheduledValues(now)
        gain.setValueAtTime(0.0001, now)
        gain.linearRampToValueAtTime(@velocity_gain, now + attack)
        gain.exponentialRampToValueAtTime(0.0001, now + duration)
      end

      private def start_one_shot(source, now, duration, generation)
        voice = self
        callback_id = source.addEventListener("ended") do |_event|
          JS::Object.removeEventListener(callback_id)
          voice.ended(generation)
        end
        @sources << source
        source.start(now)
        source.stop(now + duration)
      end
    end
  end
end
