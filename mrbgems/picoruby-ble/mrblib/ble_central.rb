class BLE
  class Central < BLE
    def initialize(debug = false)
      @debug = debug
      CYW43.init
      _init
    end
  end
end
