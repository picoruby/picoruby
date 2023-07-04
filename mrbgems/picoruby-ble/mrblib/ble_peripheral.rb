class BLE
  class Peripheral
    def initialize(profile_data)
      @connections = []
      @_read_values = {}
      @_write_values = {}
      CYW43.init
      init(profile_data)
    end

    def get_write_value(handle)
      @_write_values.delete(handle)
    end

    def set_read_value(handle, value)
      # @type var handle: untyped
      unless handle.is_a?(Integer)
        raise TypeError, "handle must be Integer"
      end
      # @type var value: untyped
      unless value.is_a?(String)
        raise TypeError, "value must be String"
      end
      @_read_values[handle] = value
    end

    def start
      hci_power_on
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

    def stop
      @task.suspend
      return 0
    end
  end
end
