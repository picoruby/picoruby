class BLE
  class Central
    def initialize
      CYW43.init
      _init
    end

    def start
      BLE.hci_power_on
      while true
        if heartbeat_on?
          heartbeat_callback
          heartbeat_off
        end
        if event_type = packet_event_type
          packet_callback(event_type)
          down_packet_flag
        end
        sleep_ms 50
      end
      return 0
    end
  end
end
