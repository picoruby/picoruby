module MIDIBASE
  class VoiceAllocator
    attr_reader :voice_count, :last_stolen

    def initialize(voices: 3)
      if voices <= 0
        raise ArgumentError, "voices must be a positive Integer"
      end
      @voice_count = voices
      @voices = Array.new(voices)
      @age = 0
      @last_stolen = nil
    end

    def allocate(channel, note, source: nil, priority: 0)
      age = @age + 1
      @age = age
      @last_stolen = nil

      voices = @voices
      voice_count = @voice_count
      free_voice = nil
      candidate_voice = nil
      candidate_priority = nil
      candidate_age = nil

      i = 0
      while i < voice_count
        entry = voices[i]
        if entry
          if entry[0] == source && entry[1] == channel && entry[2] == note
            entry[4] = age
            return i
          end

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
        elsif free_voice.nil?
          free_voice = i
        end
        i += 1
      end

      if free_voice.nil?
        voice = candidate_voice
        return nil if voice.nil?
        # @type var voice: Integer
        @last_stolen = voices[voice]
      else
        voice = free_voice
        # @type var voice: Integer
      end
      voices[voice] = [source, channel, note, priority, age]
      voice
    end

    def reserve(voice, channel, note, source: nil, priority: 0)
      return nil unless 0 <= voice && voice < @voice_count

      age = @age + 1
      @age = age
      voices = @voices
      entry = voices[voice]
      if entry && entry[0] == source && entry[1] == channel && entry[2] == note
        entry[3] = priority
        entry[4] = age
        @last_stolen = nil
      else
        @last_stolen = entry
        voices[voice] = [source, channel, note, priority, age]
      end
      voice
    end

    def release(channel, note, source: nil)
      voice = voice_for(channel, note, source: source)
      @voices[voice] = nil unless voice.nil?
      voice
    end

    def voice_for(channel, note, source: nil)
      i = 0
      voices = @voices
      voice_count = @voice_count
      while i < voice_count
        entry = voices[i]
        return i if entry && entry[0] == source && entry[1] == channel && entry[2] == note
        i += 1
      end
      nil
    end

    def entry(voice)
      return nil unless 0 <= voice && voice < @voice_count
      @voices[voice]
    end

    def release_all
      i = 0
      voices = @voices
      voice_count = @voice_count
      while i < voice_count
        voices[i] = nil
        i += 1
      end
      self
    end
  end
end
