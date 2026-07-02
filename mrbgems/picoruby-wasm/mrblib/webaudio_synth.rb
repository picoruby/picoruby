module JS
  module WebAudio
    class Synth
      attr_reader :allocator, :context, :mix_analyser

      def initialize(
            voice_count: DEFAULT_VOICE_COUNT,
            master_volume: DEFAULT_MASTER_VOLUME,
            context: nil)
        unless voice_count.is_a?(Integer) && 0 < voice_count
          raise ArgumentError, "voice_count must be a positive Integer"
        end
        unless master_volume.is_a?(Numeric) && 0.0 <= master_volume && master_volume <= 1.0
          raise ArgumentError, "master_volume must be in 0.0..1.0"
        end
        @context = context || WebAudio.create_context
        @allocator = MIDIBASE::VoiceAllocator.new(voice_count)
        @channels = {}
        @master_gain = @context.createGain
        @mix_analyser = @context.createAnalyser
        @mix_analyser[:fftSize] = WebAudio::ANALYSER_FFT_SIZE
        @master_gain[:gain].setValueAtTime(master_volume.to_f, @context[:currentTime])
        @master_gain.connect(@mix_analyser)
        @mix_analyser.connect(@context[:destination])
        @voices = []
        i = 0
        while i < voice_count
          @voices << Voice.new(self, @context, i)
          i += 1
        end
        @stopped = false
      end

      def resume
        promise = @context.resume
        promise.await if promise.respond_to?(:await)
        @stopped = false
        self
      end

      def stop
        silence_all
        @stopped = true
        self
      end

      def close
        stop
        promise = @context.close
        promise.await if promise.respond_to?(:await)
        self
      end

      def handle(event, source: DEFAULT_SOURCE, priority: 0, timestamp_us: nil, **_context)
        handle_midi(event, source, priority, timestamp_us)
      end

      def handle_midi(event, source, priority, _timestamp_us)
        return false if @stopped
        command = event[0]
        case command
        when :note_on
          note_on(source, event[1], event[2], event[3], priority)
        when :note_off
          note_off(source, event[1], event[2])
        when :control_change
          control_change(source, event[1], event[2], event[3])
        when :program_change
          program_change(source, event[1], event[2])
        when :pitch_bend
          pitch_bend(source, event[1], event[2])
        when :system_reset
          reset
        else
          false
        end
      end

      def update_tone(source: DEFAULT_SOURCE, channel:, **attributes)
        state = channel_for(source, channel)
        state.update_tone(attributes)
        update_channel_voices(source, channel, attributes.keys, false)
        state.snapshot
      end

      def channel_state(source: DEFAULT_SOURCE, channel:)
        channel_for(source, channel).snapshot
      end

      def active_voices
        result = []
        voices = @voices
        i = 0
        voices_size = voices.size
        while i < voices_size
          voice = voices[i]
          result << voice.snapshot unless voice.status == :idle
          i += 1
        end
        result
      end

      def voice_analysers
        result = []
        voices = @voices
        i = 0
        voices_size = voices.size
        while i < voices_size
          result << voices[i].analyser
          i += 1
        end
        result
      end

      def voice_ended(voice, generation)
        return unless voice.generation == generation
        @allocator.release(voice.channel.number, voice.note, source: voice.source)
        true
      end

      def reset
        silence_all
        channels = @channels.values
        i = 0
        channels_size = channels.size
        while i < channels_size
          channels[i].reset
          i += 1
        end
        true
      end

      private def note_on(source, channel_number, note, velocity, priority)
        return note_off(source, channel_number, note) if velocity <= 0
        state = channel_for(source, channel_number)
        allocator = @allocator
        voice_id = allocator.allocate(channel_number, note, source: source, priority: priority)
        return false if voice_id.nil?
        voice = @voices[voice_id]
        voice.silence if allocator.last_stolen
        voice.start(source, state, note, velocity)
        true
      end

      private def note_off(source, channel_number, note)
        voice_id = @allocator.voice_for(channel_number, note, source: source)
        return false if voice_id.nil?
        @voices[voice_id].release
        true
      end

      private def control_change(source, channel_number, controller, value)
        state = channel_for(source, channel_number)
        case controller
        when 7
          state.volume = value
        when 10
          state.pan = value
        when 11
          state.expression = value
        when 120
          all_notes(source, channel_number, true)
        when 121
          state.reset
          update_channel_voices(source, channel_number, state.tone.keys, true)
        when 123
          all_notes(source, channel_number, false)
        else
          return false
        end
        true
      end

      private def program_change(source, channel_number, program)
        state = channel_for(source, channel_number)
        state.program = program
        update_channel_voices(source, channel_number, state.tone.keys, false)
        true
      end

      private def pitch_bend(source, channel_number, value)
        state = channel_for(source, channel_number)
        state.pitch_bend = value
        update_channel_voices(source, channel_number, [], true)
        true
      end

      private def channel_for(source, channel_number)
        unless channel_number.is_a?(Integer) && 0 <= channel_number && channel_number <= 15
          raise ArgumentError, "MIDI channel must be in 0..15"
        end
        key = [source, channel_number]
        @channels[key] ||= Channel.new(@context, @master_gain, source, channel_number)
      end

      private def update_channel_voices(source, channel_number, keys, pitch)
        voices = @voices
        i = 0
        voices_size = voices.size
        while i < voices_size
          voice = voices[i]
          channel = voice.channel
          if channel && voice.source == source && channel.number == channel_number && voice.status != :idle
            voice.update_tone(keys)
            voice.update_pitch if pitch
          end
          i += 1
        end
      end

      private def all_notes(source, channel_number, immediate)
        voices = @voices
        i = 0
        voices_size = voices.size
        while i < voices_size
          voice = voices[i]
          channel = voice.channel
          if channel && voice.source == source && channel.number == channel_number && voice.status != :idle
            if immediate
              voice.silence
              @allocator.release(channel_number, voice.note, source: source)
            else
              voice.release
            end
          end
          i += 1
        end
      end

      private def silence_all
        voices = @voices
        i = 0
        voices_size = voices.size
        while i < voices_size
          voices[i].silence
          i += 1
        end
        @allocator.release_all
      end
    end
  end
end
