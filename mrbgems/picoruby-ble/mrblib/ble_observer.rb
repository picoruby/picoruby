class BLE
  class Observer < BLE::Central
    def initialize
      @role = :observer
      super(nil)
    end
  end
end

