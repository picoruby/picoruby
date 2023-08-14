class BLE
  class Broadcaster < BLE
    def initialize(profile_data = nil)
      @role = :broadcaster
      super(profile_data)
    end
  end
end

