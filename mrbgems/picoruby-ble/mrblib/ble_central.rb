class BLE
  class Central < BLE
    def initialize(profile_data = nil)
      super(profile_data)
      @found_devices = []
    end

    attr_reader :found_devices

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
