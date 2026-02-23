class BLE
  class UART < BLE
    NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
    NUS_RX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
    NUS_TX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

    # Advertising Data Types
    BLUETOOTH_DATA_TYPE_FLAGS = 0x01
    BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS = 0x07
    BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x09
    APP_AD_FLAGS = 0x06

    # BTstack event types
    BTSTACK_EVENT_STATE = 0x60
    HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
    ATT_EVENT_MTU_EXCHANGE_COMPLETE = 0xB5
    ATT_EVENT_CAN_SEND_NOW = 0xB7

    NOTIFY_MTU = 20

    def initialize(name: "PicoRuby",
                   service_uuid: NUS_SERVICE_UUID,
                   rx_uuid: NUS_RX_CHAR_UUID,
                   tx_uuid: NUS_TX_CHAR_UUID)
      service_uuid_bin = Utils.uuid(service_uuid)
      rx_uuid_bin = Utils.uuid(rx_uuid)
      tx_uuid_bin = Utils.uuid(tx_uuid)

      @adv_data = AdvertisingData.build do |a|
        a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
        a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
              Utils.reverse_128(service_uuid_bin))
        a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, name)
      end

      db = GattDatabase.new do |d|
        d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
          s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, name)
        end
        d.add_service(GATT_PRIMARY_SERVICE_UUID, service_uuid_bin) do |s|
          # RX: Central writes to this characteristic
          s.add_characteristic(
            WRITE_WITHOUT_RESPONSE | DYNAMIC,
            rx_uuid_bin,
            WRITE_WITHOUT_RESPONSE | DYNAMIC,
            ""
          )
          # TX: Peripheral notifies via this characteristic
          s.add_characteristic(
            NOTIFY | DYNAMIC,
            tx_uuid_bin,
            NOTIFY | DYNAMIC,
            ""
          ) do |c|
            c.add_descriptor(
              READ | WRITE | WRITE_WITHOUT_RESPONSE | DYNAMIC,
              CLIENT_CHARACTERISTIC_CONFIGURATION,
              "\x00\x00"
            )
          end
        end
      end

      @rx_handle = db.handle_table[service_uuid_bin][rx_uuid_bin][:value_handle]
      @tx_handle = db.handle_table[service_uuid_bin][tx_uuid_bin][:value_handle]
      @cccd_handle = db.handle_table[service_uuid_bin][tx_uuid_bin][CLIENT_CHARACTERISTIC_CONFIGURATION]

      super(:peripheral, db.profile_data)

      @rx_buffer = ""
      @tx_buffer = ""
      @notification_enabled = false
      @connected = false
    end

    def start(&block)
      @user_block = block
      super()
    end

    def heartbeat_callback
      blink_led
      _drain_rx
      _check_cccd
      _request_send
      @user_block&.call
    end

    def packet_callback(event_packet)
      case event_packet[0]&.ord
      when BTSTACK_EVENT_STATE
        return unless event_packet[2]&.ord == HCI_STATE_WORKING
        debug_puts "UART Peripheral up on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        advertise(@adv_data)
      when HCI_EVENT_DISCONNECTION_COMPLETE
        @connected = false
        @notification_enabled = false
        debug_puts "Disconnected, re-advertising"
        advertise(@adv_data)
      when ATT_EVENT_MTU_EXCHANGE_COMPLETE
        @connected = true
        debug_puts "Connected (MTU exchange complete)"
      when ATT_EVENT_CAN_SEND_NOW
        _flush_tx
      end
    end

    # --- IO interface ---

    def write(data)
      @tx_buffer << data.to_s
      _request_send
      data.to_s.bytesize
    end

    def puts(data = "")
      str = data.to_s
      write(str)
      write("\n") unless str.end_with?("\n")
      nil
    end

    def read_nonblock(nbytes = 256)
      return nil if @rx_buffer.empty?
      count = nbytes < @rx_buffer.bytesize ? nbytes : @rx_buffer.bytesize
      result = @rx_buffer.byteslice(0, count)
      @rx_buffer = @rx_buffer.byteslice(count..-1) || ""
      result
    end

    def gets_nonblock
      idx = @rx_buffer.index("\n")
      return nil unless idx
      line = @rx_buffer.byteslice(0, idx + 1)
      @rx_buffer = @rx_buffer.byteslice((idx + 1)..-1) || ""
      line
    end

    def available
      @rx_buffer.bytesize
    end

    def available?
      0 < @rx_buffer.bytesize
    end

    def connected?
      @connected
    end

    private

    def _drain_rx
      while (data = pop_write_value(@rx_handle))
        @rx_buffer << data
      end
    end

    def _check_cccd
      while (data = pop_write_value(@cccd_handle))
        @notification_enabled = (data == "\x01\x00")
        debug_puts "Notifications #{@notification_enabled ? 'enabled' : 'disabled'}"
      end
    end

    def _request_send
      if @notification_enabled && !@tx_buffer.empty?
        request_can_send_now_event
      end
    end

    def _flush_tx
      return if @tx_buffer.empty?
      chunk = @tx_buffer.byteslice(0, NOTIFY_MTU) || ""
      @tx_buffer = @tx_buffer.byteslice(NOTIFY_MTU..-1) || ""
      push_read_value(@tx_handle, chunk)
      notify(@tx_handle)
      _request_send
    end
  end
end
