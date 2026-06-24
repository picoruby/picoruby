module PSG
  TONE_K = Driver::CHIP_CLOCK / 32.0

  def self.note_to_period(note, round: true)
    frequency = 440.0 * (2.0 ** ((note - 69) / 12.0))
    period = TONE_K / frequency
    period += 0.5 if round
    period.to_i
  end
end
