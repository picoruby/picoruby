require 'rmt'

class SK6812
  def initialize(gpio)
    @rmt = RMT.new(gpio, t0h_ns: 300, t0l_ns: 900, t1h_ns: 600, t1l_ns: 600, reset_ns: 40000)
  end

  def output(r: 0, g: 0, b: 0)
     @rmt.write([g, r, b])
  end
end
