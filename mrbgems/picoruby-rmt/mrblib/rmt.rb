class RMT
  def initialize(pin, t0h_ns: 0, t0l_ns: 0, t1h_ns: 0, t1l_ns: 0, reset_ns: 0)
    _init(pin, t0h_ns, t0l_ns, t1h_ns, t1l_ns, reset_ns)
  end

  def write(*params)
    _write(params_to_array(*params))
  end

  # private

  def params_to_array(*params)
    ary = []
    params.each do |param|
      case param
      when Array
        # @type var param: Array[Integer]
        ary += param
      when Integer
        ary << param
      when String
        # @type var param: String
        ary += param.bytes
      end
    end
    return ary
  end
end
