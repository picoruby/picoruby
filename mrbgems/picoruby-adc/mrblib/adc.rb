require "gpio"

class ADC
  def initialize(pin, params = {})
    # @params = params
    @min = nil
    @max = nil
    @input = -1
    ADC._init(pin)
  end
end

