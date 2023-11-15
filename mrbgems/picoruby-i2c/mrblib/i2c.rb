require "gpio"

class I2C
  DEFAULT_FREQUENCY = 100_000

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sda_pin: -1, scl_pin: -1)
    @unit_num = _init(unit.to_s, frequency, sda_pin, scl_pin)
  end

  def read(i2c_adrs_7, len, *outputs)
    unless outputs.empty?
       _write(@unit_num, i2c_adrs_7, outputs_array(outputs), true)
    end
    ret = _read(@unit_num, i2c_adrs_7, len)
    return ret if String === ret
    IOError.peripheral_error(ret, "I2C#read")
    return ""
  end

  def write(i2c_adrs_7, *outputs)
    ret = _write(@unit_num, i2c_adrs_7, outputs_array(outputs), false)
    return ret if 0 < ret
    IOError.peripheral_error(ret, "I2C#write")
  end

  def outputs_array(outputs)
    ary = []
    outputs.each do |param|
      case param
      when Array
        ary += param
      when Integer
        ary << param
      when String
        ary += param.bytes
      end
    end
    ary
  end

  def scan
    msg_format = "I2C device found at 7-bit address 0x%02x (0b%07b) +%s\n"
    (0x08..0x77).each do |i2c_adrs_7|
      begin
        read(i2c_adrs_7, 1)
        printf(msg_format, i2c_adrs_7, i2c_adrs_7, "R")
      rescue IOError
        begin
          write(i2c_adrs_7, 0)
          printf(msg_format, i2c_adrs_7, i2c_adrs_7, "W")
        rescue IOError
          # ignore
        end
      end
    end
    nil
  end
end
