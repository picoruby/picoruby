require "gpio"

class I2C
  DEFAULT_FREQUENCY = 100_000

  def initialize(params)
    @unit_num = _init(
      params[:unit].to_s,
      params[:frequency] || DEFAULT_FREQUENCY,
      params[:sda_pin],
      params[:scl_pin]
    )
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
        param.each do |e|
          ary << e
        end
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
