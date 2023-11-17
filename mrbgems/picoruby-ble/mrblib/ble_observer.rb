class BLE
  class Observer < BLE::Central
    def initialize
      super(nil)
      @role = :observer
    end
  end
end

