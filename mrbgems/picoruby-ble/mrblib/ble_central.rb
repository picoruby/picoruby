class BLE
  class Central < BLE
    def initialize(profile_data = nil)
      super(profile_data)
      @found_devices = []
      reset_state
      @services = []
    end

    attr_reader :found_devices, :services, :state

    def reset_state
      @state = :TC_OFF
    end

    def connect(device_id)
      unless device = @found_devices[device_id]
        puts "No device with id #{device_id} found."
        return false
      end
      stop_scan # Is it necessary?
      @_event_packets.clear
      err_code = gap_connect(device.address, device.address_type)
      if err_code == 0
        @state = :TC_W4_CONNECT
        start(10, :TC_W4_READY)
        return true
      else
        puts "Error: #{err_code}"
        return false
      end
    end

    def add_found_device(adv_report)
      @found_devices << adv_report
    end

    def clear_found_devices
      @found_devices = []
    end

    def print_found_devices
      @found_devices.each_with_index do |adv_report, index|
        puts "ID: #{index}"
        puts adv_report.format
        puts ""
      end
      nil
    end

  end
end
