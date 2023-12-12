class PWM
  def initialize(pin, frequency: 0, duty: 50)
    @slice_num = _init(pin)
    @duty = duty.to_f
    if 0 < frequency
      _set_frequency_and_duty(@slice_num, frequency.to_f, @duty)
    end
  end

  def frequence(value)
    if 0 < value
      _set_frequency_and_duty(@slice_num, value.to_f, @duty)
      _start(@slice_num)
    else
      _stop(@slice_num)
    end
  end
end
