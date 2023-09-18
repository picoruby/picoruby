module Kernel
  alias :require_bak :require
  def require(gem)
    begin
      require_bak(gem)
    rescue LoadError
    end
  end
end

require_relative "../mrblib/ble.rb"
require_relative "../mrblib/ble_advertising_data.rb"

  APP_AD_FLAGS = 0x06
  BLUETOOTH_DATA_TYPE_FLAGS = 0x01
  BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS = 0x03
  BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09
@adv_data = BLE::AdvertisingData.build do |a|
  a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
  a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, "PicoRuby BLE")
  a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x181a)
end

puts @adv_data.b.inspect
