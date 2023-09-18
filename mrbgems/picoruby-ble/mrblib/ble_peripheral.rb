class BLE
  class Peripheral < BLE
    def initialize(profile_data = nil)
      @role = :peripheral
      super(profile_data)
    end
  end
end
