class Co2
  def initialize
    @value = 0
  end
  def concentrate
    res = get_co2
    if res[0] == 255 && res[1] == 134
      res[2] * 256 + res[3]
    else
      0
    end
  end
end
