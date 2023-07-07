class BLE
  class Central < BLE
    def initialize(debug = false)
      @debug = debug
      @_event_packets = []
      @_read_values = {}
      @_write_values = {}
      CYW43.init
      _init
    end
  end
end
