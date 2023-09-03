require "gpio"

class SPI
  MSB_FIRST = 1
  LSB_FIRST = 0
  DATA_BITS = 8
  DEFAULT_FREQUENCY = 100_000

  def initialize(unit:, frequency: DEFAULT_FREQUENCY, sck_pin: -1, cipo_pin: -1, copi_pin: -1, cs_pin: -1, mode: 0, first_bit: MSB_FIRST)
    @unit     = unit.to_s
    @sck_pin  = sck_pin
    @cipo_pin = cipo_pin
    @copi_pin = copi_pin
    @cs_pin   = cs_pin
    @unit_num = _init(
      @unit,
      frequency,
      @sck_pin,
      @cipo_pin,
      @copi_pin,
      mode,
      first_bit,
      DATA_BITS # Data bit size. No support other than 8
    )
    if -1 < @cs_pin
      @cs = GPIO.new(@cs_pin, GPIO::OUT)
      @cs.write(1)
    end
  end

  attr_reader :unit, :sck_pin, :cipo_pin, :copi_pin, :cs_pin

  def select
    @cs&.write(0)
    if block_given?
      begin
        yield(self)
      ensure
        deselect
      end
    end
  end

  def deselect
    @cs&.write(1)
  end

  def read(len, repeated_tx_data = 0)
    ret = _read(@unit_num, len, repeated_tx_data)
    return ret if String === ret
    GPIO.handle_error(ret, "SPI#read")
    return ""
  end

  def write(*params)
    ret = _write(@unit_num, params_to_array(*params))
    return ret if -1 < ret
    GPIO.handle_error(ret, "SPI#write")
  end

  def transfer(*params, additional_read_bytes: 0)
    ary = params_to_array(*params)
    additional_read_bytes.times do
      ary << 0
    end
    ret = _transfer(@unit_num, params_to_array(*params))
    return ret if String === ret
    GPIO.handle_error(ret, "SPI#transfer")
    return ""
  end

  # private

  def params_to_array(*params)
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
    return ary
  end
end
