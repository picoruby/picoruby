class BLE
  class Central < BLE
    def initialize(debug = false)
      @debug = debug
      @_events = []
      CYW43.init
      _init
    end
  end
end
