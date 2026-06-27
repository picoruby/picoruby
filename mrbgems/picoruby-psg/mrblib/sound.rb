module PSG
  @sound_table = {} #: Hash[Symbol, Sound]
  @drum_sound_map = {} #: Hash[Integer, Symbol]

  class << self
    def set_sound_data(name, data)
      unless name.is_a?(Symbol)
        raise TypeError, "PSG sound name must be a Symbol: #{name.inspect}"
      end
      sound = Sound.new(data)
      @sound_table[name] = sound
      sound
    end

    def sound_data(name)
      sound = sound_for(name, true)
      raise ArgumentError, "Unknown PSG sound: #{name.inspect}" if sound.nil?
      sound.data
    end

    def sound_duration(value)
      sound = sound_for(value, value.is_a?(Symbol))
      sound ? sound.duration : 0
    end

    def assign_drum_sound(note, sound_name)
      unless note.is_a?(Integer) && 0 <= note && note <= 127
        raise ArgumentError, "MIDI drum note must be in 0..127: #{note.inspect}"
      end
      drum_sound_map = @drum_sound_map
      if sound_name.nil?
        drum_sound_map.delete(note)
        return nil
      end
      unless sound_name.is_a?(Symbol)
        raise TypeError, "MIDI drum sound name must be a Symbol: #{sound_name.inspect}"
      end
      unless @sound_table[sound_name]
        raise ArgumentError, "Unknown PSG sound: #{sound_name.inspect}"
      end
      drum_sound_map[note] = sound_name
      sound_name
    end

    private

    def sound_for(value, raise_unknown)
      return value if value.is_a?(Sound)

      name = if value.is_a?(Symbol)
               value
             elsif value.is_a?(Integer)
               @drum_sound_map[value]
             else
               raise TypeError, "PSG sound must be a Symbol, Integer, or PSG::Sound: #{value.inspect}"
             end
      sound = name && @sound_table[name]
      if sound.nil? && raise_unknown
        raise ArgumentError, "Unknown PSG sound: #{name.inspect}"
      end
      sound
    end
  end

  # [tone_period, noise_period(1..15), volume(0..15), duration_ms]
  set_sound_data(:kick, [
    [ 720, 12, 15, 25],
    [ 960, 14, 13, 50],
    [1200,  0,  8, 30]
  ])
  set_sound_data(:snare, [
    [200, 4, 15, 30],
    [210, 5, 13, 40],
    [  0, 6,  9, 50]
  ])
  set_sound_data(:closed_hihat, [
    [0, 1, 13, 18],
    [0, 2, 10, 20]
  ])
  set_sound_data(:open_hihat, [
    [0, 2, 13,  18],
    [0, 1, 11, 300],
  ])
  set_sound_data(:low_tom, [
    [300, 0, 15,  45],
    [360, 0, 12,  65],
    [460, 0,  7, 110]
  ])
  set_sound_data(:mid_tom, [
    [240, 0, 15, 40],
    [300, 0, 12, 60],
    [390, 0,  7, 90]
  ])
  set_sound_data(:high_tom, [
    [190, 0, 15, 35],
    [240, 0, 12, 55],
    [320, 0,  7, 80]
  ])

  assign_drum_sound(35, :kick)
  assign_drum_sound(36, :kick)
  assign_drum_sound(38, :snare)
  assign_drum_sound(40, :snare)
  assign_drum_sound(42, :closed_hihat)
  assign_drum_sound(44, :closed_hihat)
  assign_drum_sound(46, :open_hihat)
  assign_drum_sound(41, :low_tom)
  assign_drum_sound(43, :low_tom)
  assign_drum_sound(45, :mid_tom)
  assign_drum_sound(47, :mid_tom)
  assign_drum_sound(48, :high_tom)
  assign_drum_sound(50, :high_tom)
end
