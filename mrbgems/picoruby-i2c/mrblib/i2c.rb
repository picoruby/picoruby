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

  def poll(i2c_adrs_7)
    _poll(@unit_num, i2c_adrs_7)
  end

  def write(i2c_adrs_7, *params)
    case params[0]
    when Array
      _write(@unit_num, i2c_adrs_7, params[0]) # Ignore subsequent values
    else
      ary = []
      params.each do |param|
        case param
        when Integer
          ary << param
        when String
          param.each_byte do |byte|
            ary << byte
          end
        end
      end
      _write(@unit_num, i2c_adrs_7, ary)
    end
  end
end
