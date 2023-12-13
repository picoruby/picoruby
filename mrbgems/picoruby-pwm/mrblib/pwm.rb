class PWM
  def initialize(pin, frequency: 0, duty: 50)
    @gpio = pin
    _init(@gpio)
    @frequency = frequency.to_f
    @duty = duty.to_f
    frequency(@frequency)
  end
end
