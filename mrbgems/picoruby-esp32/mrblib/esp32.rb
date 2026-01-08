class ESP32
  class WiFi
    LINK_DOWN  = 0
    LINK_NOIP  = 2
    LINK_UP    = 3
    LINK_NONET = 5

    def self.tcpip_link_status_name
      case tcpip_link_status
      when LINK_DOWN
        "LINK_DOWN"
      when LINK_NOIP
        "LINK_NOIP"
      when LINK_UP
        "LINK_UP"
      when LINK_NONET
        "LINK_NONET"
      else
        "UNKNOWN_STATUS"
      end
    end
  end

  class Auth
    OPEN           = 0
    WPA_TKIP_PSK   = 2
    WPA2_AES_PSK   = 3
    WPA2_MIXED_PSK = 4
  end

  # Exception raised when WiFi connection times out
  class ConnectTimeout < RuntimeError
  end
end
