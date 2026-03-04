class ESP32
  class WiFi
    class Auth
      OPEN           = 0
      WPA_TKIP_PSK   = 2
      WPA2_AES_PSK   = 3
      WPA2_MIXED_PSK = 4
    end

    LINK_DOWN  = 0
    LINK_NOIP  = 2
    LINK_UP    = 3
    LINK_NONET = 5

    def self.init(country = nil, force: false)
      if initialized?
        puts "ESP32::WiFi already initialized"
        return true
      end
      unless _init
        puts "ESP32::WiFi.init failed. No ESP32 module is connected?"
        return false
      end
      return true
    end

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

    def self.link_connected?
      tcpip_link_status == ESP32::WiFi::LINK_UP
    end

    def self.enable_sta_mode
      # TODO: Extract the process for setting the sta mode from ESP32_WIFI_init()
      true
    end
  end

  # Exception raised when WiFi connection times out
  class ConnectTimeout < RuntimeError
  end
end
