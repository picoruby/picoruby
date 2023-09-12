class CYW43
  def self.init(country = "XX", force: false)
    if (!country.is_a?(String) || country.length != 2)
      raise ArgumentError, "country must be a 2-character string"
    end
    if self._init(country.upcase, force)
      $_cyw43_country = country
    end
  end

  def self.check_initialized
    unless $_cyw43_country
      raise RuntimeError, "CYW43.init must be called before using GPIO"
    end
  end

  class GPIO
    LED_PIN = 0
    def initialize(pin)
      CYW43.check_initialized
      @pin = pin
    end
  end
end
