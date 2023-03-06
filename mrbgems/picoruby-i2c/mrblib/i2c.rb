require "gpio"

class I2C
  DEFAULT_FREQUENCY = 100_000

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sda_pin: -1, scl_pin: -1)
    @unit_num = _init(unit.to_s, frequency, sda_pin, scl_pin)
  end

  def read(i2c_adrs_7, len)
    ret = _read(@unit_num, i2c_adrs_7, len)
    return ret if String === ret
    GPIO.handle_error(ret, "I2C#read")
    return ""
  end

  def write(i2c_adrs_7, *params)
    ary = []
    params.each do |param|
      case param
      when Array
        ary += param
      when Integer
        ary << param
      when String
        art += param.bytes
      end
    end
    ret = _write(@unit_num, i2c_adrs_7, ary)
    return ret if 0 < ret
    GPIO.handle_error(ret, "I2C#write")
  end
end
