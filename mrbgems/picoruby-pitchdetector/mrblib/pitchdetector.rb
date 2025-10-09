# Usage: See example/detect_pitch.rb

require 'adc'

class PitchDetector
  def initialize(pin)
    adc = ADC.new(pin)
    @adc_input = adc.input
    self.volume_threshold = 300
  end

  class Note
    # Reference frequency: A4 = 440Hz
    A4_FREQUENCY = 440.0
    # Note names
    NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

    def initialize(reference = A4_FREQUENCY)
      @a4_frequency = reference.to_f
    end

    def freq_to_note(frequency)
      # Calculate semitones from A4
      # n = 12 * log2(f/440)
      semitones_from_a4 = 12 * Math.log2(frequency / @a4_frequency)
      # Round to nearest semitone
      rounded_semitones = semitones_from_a4.round
      # Calculate note index (A = 9 in NAMES)
      note_index = (9 + rounded_semitones) % 12
      # Calculate octave number (A4 = octave 4)
      octave = 4 + (9 + rounded_semitones) / 12
      # Get note name
      note_name = NAMES[note_index]
      # Calculate cents (pitch deviation)
      cents = ((semitones_from_a4 - rounded_semitones) * 100).round
      {
        octave: octave.to_i,
        note: note_name,
        cents: cents,
        frequency: sprintf('%.1f', frequency)
      }
    end

    def note_to_freq(note_name, octave)
      # Get note index from note name
      note_index = NAMES.index(note_name.upcase)
      return nil unless note_index
      # Calculate semitones from A4
      semitones_from_a4 = (octave - 4) * 12 + (note_index - 9)
      # Calculate frequency
      @a4_frequency * (2.0 ** (semitones_from_a4 / 12.0))
    end
  end
end
