class CYW43
  def self.init(country = "XX", force: false)
    if (!country.is_a?(String) || country.length != 2)
      raise ArgumentError, "country must be a 2-character string"
    end
    if self._init(country.upcase, force)
      $_cyw43_country = country
    end
  end

  def self.initialized?
    !!$_cyw43_country
  end

  class GPIO
    LED_PIN = 0
    def initialize(pin)
      CYW43.init unless CYW43.initialized?
      @pin = pin
    end
  end

  class Auth
    OPEN           = 0
    WPA_TKIP_PSK   = 0x00200002
    WPA2_AES_PSK   = 0x00400004
    WPA2_MIXED_PSK = 0x00400006
  end
end
