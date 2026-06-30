module PSG
  MAX_VOICE_PROGRAM_STEPS = 16

  @voice_programs = {} #: Hash[Symbol, Array[Array[Integer]]]
  @drum_program_map = {} #: Hash[Integer, Symbol]

  class << self
    def define_voice_program(name, steps)
      unless name.is_a?(Symbol)
        raise TypeError, "voice program name must be a Symbol: #{name.inspect}"
      end
      unless steps.is_a?(Array) && 0 < steps.size && steps.size <= MAX_VOICE_PROGRAM_STEPS
        raise ArgumentError, "voice program must have 1..#{MAX_VOICE_PROGRAM_STEPS} steps"
      end

      program = [] #: voice_program_t
      i = 0
      steps_size = steps.size
      while i < steps_size
        step = steps[i]
        unless step.is_a?(Array) && step.size == 4
          raise ArgumentError, "voice program step must be [tone_period, noise_period, volume, duration_ms]: #{step.inspect}"
        end
        tone_period = step[0]
        noise_period = step[1]
        volume = step[2]
        duration_ms = step[3]
        unless tone_period.is_a?(Integer) && noise_period.is_a?(Integer) &&
               volume.is_a?(Integer) && duration_ms.is_a?(Integer)
          raise TypeError, "voice program step values must be Integers: #{step.inspect}"
        end
        unless 0 <= tone_period && tone_period <= 0x0FFF &&
               0 <= noise_period && noise_period <= 31 &&
               0 <= volume && volume <= 15 && 0 <= duration_ms
          raise ArgumentError, "invalid voice program step: #{step.inspect}"
        end
        program_step = [tone_period, noise_period, volume, duration_ms] #: voice_program_step_t
        program_step.freeze
        program << program_step
        i += 1
      end
      program.freeze
      @voice_programs[name] = program
      program
    end

    def voice_program(name)
      program = @voice_programs[name]
      raise ArgumentError, "unknown voice program: #{name.inspect}" if program.nil?
      program
    end

    def assign_drum_program(note, program_name)
      unless note.is_a?(Integer) && 0 <= note && note <= 127
        raise ArgumentError, "MIDI drum note must be in 0..127: #{note.inspect}"
      end
      drum_program_map = @drum_program_map
      if program_name.nil?
        drum_program_map.delete(note)
        return nil
      end
      unless program_name.is_a?(Symbol)
        raise TypeError, "voice program name must be a Symbol: #{program_name.inspect}"
      end
      voice_program(program_name)
      drum_program_map[note] = program_name
      program_name
    end

    def drum_program(note)
      name = @drum_program_map[note]
      name ? @voice_programs[name] : nil
    end
  end

  # [tone_period, noise_period(1..31), volume(0..15), duration_ms]
  define_voice_program(:kick, [
    [ 820, 12, 15, 25],
    [1080, 14, 13, 50],
    [1200,  0,  8, 30]
  ])
  define_voice_program(:snare, [
    [180, 4, 15, 30],
    [  0, 5, 13, 40],
    [  0, 6,  9, 40]
  ])
  define_voice_program(:closed_hihat, [
    [0, 1, 13, 18],
    [0, 2, 10, 20]
  ])
  define_voice_program(:open_hihat, [
    [0, 2, 13,  18],
    [0, 1,  9, 200],
  ])
  define_voice_program(:low_tom, [
    [400, 0, 15,  45],
    [460, 0, 12,  65],
    [560, 0,  7, 110]
  ])
  define_voice_program(:mid_tom, [
    [320, 0, 15, 40],
    [380, 0, 12, 60],
    [470, 0,  7, 90]
  ])
  define_voice_program(:high_tom, [
    [240, 0, 15, 35],
    [290, 0, 12, 55],
    [370, 0,  7, 80]
  ])

  assign_drum_program(35, :kick)
  assign_drum_program(36, :kick)
  assign_drum_program(38, :snare)
  assign_drum_program(40, :snare)
  assign_drum_program(42, :closed_hihat)
  assign_drum_program(44, :closed_hihat)
  assign_drum_program(46, :open_hihat)
  assign_drum_program(41, :low_tom)
  assign_drum_program(43, :low_tom)
  assign_drum_program(45, :mid_tom)
  assign_drum_program(47, :mid_tom)
  assign_drum_program(48, :high_tom)
  assign_drum_program(50, :high_tom)
end
