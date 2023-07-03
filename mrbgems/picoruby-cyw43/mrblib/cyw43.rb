class CYW43
  def self.init(country = "XX", force: false)
    if (!country.is_a?(String) || country.length != 2)
      raise ArgumentError, "country must be a 2-character string"
    end
    self._init(country.upcase, force)
  end

  class GPIO
    LED_PIN = 0
    def initialize(pin)
      @pin = pin
    end
  end
end
