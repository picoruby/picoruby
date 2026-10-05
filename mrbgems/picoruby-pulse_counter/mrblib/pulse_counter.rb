class PulseCounter
  def initialize(pin_a, pin_b, glitch_ns: 1000, pull_up: true)
    @unit = _init(pin_a, pin_b, glitch_ns, pull_up)
    if @unit < 0
      raise RuntimeError.new("PulseCounter: failed to initialize (pin_a: #{pin_a}, pin_b: #{pin_b})")
    end
  end

  def count
    _count(@unit)
  end

  def clear
    _clear(@unit)
    nil
  end
end
