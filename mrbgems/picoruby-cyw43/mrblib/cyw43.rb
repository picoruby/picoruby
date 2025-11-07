require 'env'

class CYW43
  def self.init(country = nil, force: false)
    if CYW43.initialized?
      puts "CYW43 already initialized"
      return true
    end
    if country.is_a?(String) && country.length != 2
      puts "country must be a 2-character string"
      return false
    end
    unless self._init(country&.upcase, force)
      puts "CYW43.init failed. No CYW43 module is connected?"
      return false
    end
    ENV['WIFI_MODULE'] = "CWY43"
    return true
  end

  def self.tcpip_link_status_name
    case tcpip_link_status
    when LINK_DOWN
      "LINK_DOWN"
    when LINK_JOIN
      "LINK_JOIN"
    when LINK_NOIP
      "LINK_NOIP"
    when LINK_UP
      "LINK_UP"
    when LINK_FAIL
      "LINK_FAIL"
    when LINK_NONET
      "LINK_NONET"
    when LINK_BADAUTH
      "LINK_BADAUTH"
    else
      "UNKNOWN_STATUS"
    end
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
