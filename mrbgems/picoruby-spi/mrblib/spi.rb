require "gpio"

class SPI
  MSB_FIRST = 1
  LSB_FIRST = 0
  DATA_BITS = 8
  DEFAULT_FREQUENCY = 100_000

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sck_pin: -1, cipo_pin: -1, copi_pin: -1, mode: 0, first_bit: MSB_FIRST)
    @unit_num = _init(
      unit.to_s,
      frequency,
      sck_pin,
      cipo_pin,
      copi_pin,
      mode,
      first_bit,
      DATA_BITS # Data bit size. No support other than 8
    )
  end

  def read(len, repeated_tx_data = 0)
    ret = _read(@unit_num, len, repeated_tx_data)
    return ret if String === ret
    GPIO.handle_error(ret, "SPI#read")
    return ""
  end

  def write(*params)
    ary = []
    params.each do |param|
      case param
      when Array
        ary += param
      when Integer
        ary << param
      when String
        ary += param.bytes
      end
    end
    ret = _write(@unit_num, ary)
    return ret if -1 < ret
    GPIO.handle_error(ret, "SPI#write")
  end
end
