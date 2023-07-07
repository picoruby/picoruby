class BLE
  class Central < BLE
    def initialize(debug = false)
      @debug = debug
      @_event_packets = []
      CYW43.init
      _init
    end
  end
end
