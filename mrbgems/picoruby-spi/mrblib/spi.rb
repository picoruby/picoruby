require "gpio"

class SPI
  MSB_FIRST = 1
  LSB_FIRST = 0
  DATA_BITS = 8
  DEFAULT_FREQUENCY = 100_000

  def initialize(params)
    @unit_num = _init(
      params[:unit].to_s,
      params[:frequency] || DEFAULT_FREQUENCY,
      params[:sck_pin]   || -1,
      params[:cipo_pin]  || -1,
      params[:copi_pin]  || -1,
      params[:mode]      || 0,
      params[:first_bit] || MSB_FIRST,
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
