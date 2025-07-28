# Usage: See example/detect_pitch.rb

require 'adc'

class PitchDetector
  def initialize(pin, volume_threshold: 200)
    adc = ADC.new(pin)
    @adc_input = adc.input
    self.volume_threshold = volume_threshold
  end
end
