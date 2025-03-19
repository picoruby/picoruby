class CYW43
  def self.init(country = nil, force: false)
    if country.is_a?(String) && country.length != 2
      raise ArgumentError, "country must be a 2-character string"
    end
    self._init(country&.upcase, force)
  end

  def self.link_connected?(print_status = false)
    result = false
    status = case tcpip_link_status
    when LINK_DOWN
      "link down"
    when LINK_JOIN
      "link join"
    when LINK_NOIP
      "link no ip"
    when LINK_UP
      result = true
      "link up"
    when LINK_FAIL
      "link fail"
    when LINK_NONET
      "link no net"
    when LINK_BADAUTH
      "link bad auth"
    else
      "unknown status"
    end
    puts "TCP/IP link status: #{status}" if print_status
    result
  end


  class GPIO
    LED_PIN = 0
    def initialize(pin)
      unless CYW43.initialized?
        raise RuntimeError, "CYW43 not initialized"
      end
      @pin = pin
    end

    def high?
      read == 1
    end

    def low?
      read == 0
    end
  end

  class Auth
    OPEN           = 0
    WPA_TKIP_PSK   = 0x00200002
    WPA2_AES_PSK   = 0x00400004
    WPA2_MIXED_PSK = 0x00400006
  end
end
