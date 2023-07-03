class MyServer < BLE::Peripheral
  # for advertising
  APP_AD_FLAGS = 0x06
  BLUETOOTH_DATA_TYPE_FLAGS = 0x01
  BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS = 0x03
  BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09
  # for GATT
  CYW43_WL_GPIO_LED_PIN = BLE::CYW43_WL_GPIO_LED_PIN
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
  ATT_EVENT_CAN_SEND_NOW = 0xB7
  ATT_EVENT_DISCONNECTED = 0xB4
  ATT_EVENT_MTU_EXCHANGE_COMPLETE = 0xB5
  #
  SERVICE_ENVIRONMENTAL_SENSING = 0x181A
  CHARACTERISTIC_TEMPERATURE = 0x2A6E

  def initialize(debug: false)
    @debug = debug
    db = BLE::GattDatabase.new do |db|
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, BLE::GAP_SERVICE_UUID) do |s|
        s.add_characteristic(BLE::GAP_DEVICE_NAME_UUID, BLE::READ, "picoR_temp")
      end
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, BLE::GATT_SERVICE_UUID) do |s|
        s.add_characteristic(BLE::CHARACTERISTIC_DATABASE_HASH, BLE::READ)
      end
      db.add_service(BLE::GATT_PRIMARY_SERVICE_UUID, SERVICE_ENVIRONMENTAL_SENSING) do |s|
        s.add_characteristic(CHARACTERISTIC_TEMPERATURE, BLE::READ|BLE::NOTIFY|BLE::INDICATE|BLE::DYNAMIC)
      end
    end
    @temperature_handle = db.handle_table[:characteristic][:value][CHARACTERISTIC_TEMPERATURE]
    super(db.profile_data)
    @last_event = 0
    @led_on = false
    @counter = 0
    @adv_data = BLE::AdvertisingData.build do |a|
      a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, "PicoRuby BLE")
      a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x181a)
    end
    @temperature = 0
  end

  def debug_puts(*args)
    puts(*args) if @debug
  end

  def heartbeat_callback
    @counter += 1
    @temperature += 1
    save_read_value(@temperature_handle, BLE::Utils.int16_to_little_endian(@temperature))
    if @counter == 10
      if notification_enabled?
        debug_puts "notification_enabled"
        request_can_send_now_event
      end
      @counter = 0
    end
    case @last_event
    when BTSTACK_EVENT_STATE, HCI_EVENT_DISCONNECTION_COMPLETE, ATT_EVENT_DISCONNECTED
      @led_on = !@led_on
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, @led_on);
    end
  end

  def packet_callback(event_type)
    debug_puts "event type: #{sprintf "%02X", event_type}"
    @last_event = event_type
    case event_type
    when BTSTACK_EVENT_STATE
      puts "Peripheral is up and running on: `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
      advertise(@adv_data)
    when HCI_EVENT_DISCONNECTION_COMPLETE, ATT_EVENT_DISCONNECTED
      debug_puts "disconnected"
      disable_notification
    when ATT_EVENT_MTU_EXCHANGE_COMPLETE
      debug_puts "mtu exchange complete"
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    when ATT_EVENT_CAN_SEND_NOW
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
      notify @temperature_handle
      sleep_ms 10
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    end
  end
end

