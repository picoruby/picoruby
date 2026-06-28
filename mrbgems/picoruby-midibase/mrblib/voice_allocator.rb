module MIDIBASE
  class VoiceAllocator
    def initialize(voice_count = 3)
      if voice_count <= 0
        raise ArgumentError, "voices must be a positive Integer"
      end
      @voices = Array.new(voice_count)
      @age = 0
      @last_stolen = nil
    end

    attr_reader :last_stolen

    def voice_count
      @voices.size
    end

    def allocate(channel, note, source: nil, priority: 0, voice_ids: nil)
      age = @age + 1
      @age = age
      @last_stolen = nil

      voices = @voices
      voice_count = voices.size
      free_voice = nil
      candidate_voice = nil
      candidate_priority = nil
      candidate_age = nil

      i = 0
      candidate_count = voice_ids ? voice_ids.size : voice_count
      while i < candidate_count
        voice_id = voice_ids ? voice_ids[i] : i
        unless voice_id && 0 <= voice_id && voice_id < voice_count
          i += 1
          next
        end
        entry = voices[voice_id]
        if entry
          if entry[0] == source && entry[1] == channel && entry[2] == note
            entry[4] = age
            return voice_id
          end

          # @type var entry: Array[untyped]
          entry_priority = entry[3]
          entry_age = entry[4]
          if entry_priority <= priority &&
             (candidate_priority.nil? || entry_priority < candidate_priority ||
              (entry_priority == candidate_priority && entry_age < candidate_age))
            candidate_voice = voice_id
            candidate_priority = entry_priority
            candidate_age = entry_age
          end
        elsif free_voice.nil?
          free_voice = voice_id
        end
        i += 1
      end

      if free_voice.nil?
        voice_id = candidate_voice
        return nil if voice_id.nil?
        # @type var voice_id: Integer
        @last_stolen = voices[voice_id]
      else
        voice_id = free_voice
        # @type var voice: Integer
      end
      voices[voice_id] = [source, channel, note, priority, age]
      voice_id
    end

    def reserve(id, channel, note, source: nil, priority: 0)
      return nil unless 0 <= id && id < @voices.size

      age = @age + 1
      @age = age
      voices = @voices
      entry = voices[id]
      if entry && entry[0] == source && entry[1] == channel && entry[2] == note
        entry[3] = priority
        entry[4] = age
        @last_stolen = nil
      else
        @last_stolen = entry
        voices[id] = [source, channel, note, priority, age]
      end
      id
    end

    def release(channel, note, source: nil)
      voice = voice_for(channel, note, source: source)
      @voices[voice] = nil unless voice.nil?
      voice
    end

    def voice_for(channel, note, source: nil)
      id = 0
      voices = @voices
      voice_count = voices.size
      while id < voice_count
        entry = voices[id]
        return id if entry && entry[0] == source && entry[1] == channel && entry[2] == note
        id += 1
      end
      nil
    end

    def entry(id)
      id < 0 ? nil : @voices[id]
    end

    def release_all
      i = 0
      voices = @voices
      voice_count = voices.size
      while i < voice_count
        voices[i] = nil
        i += 1
      end
      self
    end
  end
end
