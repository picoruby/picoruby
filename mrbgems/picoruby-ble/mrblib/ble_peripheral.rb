class BLE
  class Peripheral < BLE
    def initialize(profile_data, debug = false)
      @debug = debug
      @_read_values = {}
      @_write_values = {}
      @_event_packets = []
      CYW43.init
      _init(profile_data)
    end
  end
end
