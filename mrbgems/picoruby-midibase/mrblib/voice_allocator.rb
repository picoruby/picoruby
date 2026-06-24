module MIDIBASE
  class VoiceAllocator
    attr_reader :voice_count, :last_stolen

    def initialize(voices: 3)
      unless voices.is_a?(Integer) && 0 < voices
        raise ArgumentError, "voices must be a positive Integer"
      end
      @voice_count = voices
      @voices = Array.new(voices)
      @age = 0
      @last_stolen = nil
    end

    def allocate(channel, note, source: nil, priority: 0)
      @age += 1
      @last_stolen = nil
      voice = voice_for(channel, note, source: source)
      unless voice.nil?
        entry = @voices[voice]
        # @type var entry: Array[untyped]
        entry[4] = @age
        return voice
      end

      voice = @voices.index(nil)
      if voice.nil?
        candidate_voice = nil
        candidate_priority = nil
        candidate_age = nil
        i = 0
        while i < @voices.size
          entry = @voices[i]
          # @type var entry: Array[untyped]
          entry_priority = entry[3]
          entry_age = entry[4]
          if entry_priority <= priority &&
             (candidate_priority.nil? || entry_priority < candidate_priority ||
              (entry_priority == candidate_priority && entry_age < candidate_age))
            candidate_voice = i
            candidate_priority = entry_priority
            candidate_age = entry_age
          end
          i += 1
        end
        voice = candidate_voice
      end
      return nil if voice.nil?

      @last_stolen = @voices[voice]
      @voices[voice] = [source, channel, note, priority, @age]
      voice
    end

    def release(channel, note, source: nil)
      voice = voice_for(channel, note, source: source)
      @voices[voice] = nil unless voice.nil?
      voice
    end

    def voice_for(channel, note, source: nil)
      i = 0
      while i < @voices.size
        entry = @voices[i]
        return i if entry && entry[0] == source && entry[1] == channel && entry[2] == note
        i += 1
      end
      nil
    end

    def entry(voice)
      return nil unless voice.is_a?(Integer) && 0 <= voice && voice < @voices.size
      @voices[voice]
    end

    def release_all
      i = 0
      while i < @voices.size
        @voices[i] = nil
        i += 1
      end
      self
    end
  end
end
