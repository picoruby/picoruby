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

    def initialize(role: :peripheral,
                   name: "RubyUART",
                   service_uuid: NUS_SERVICE_UUID,
                   rx_uuid: NUS_RX_CHAR_UUID,
                   tx_uuid: NUS_TX_CHAR_UUID)
      @service_uuid_bin = Utils.uuid(service_uuid)
      @rx_uuid_bin = Utils.uuid(rx_uuid)
      @tx_uuid_bin = Utils.uuid(tx_uuid)
      @rx_buffer = ""
      @tx_buffer = ""
      @connected = false

      if role == :peripheral
        adv_data = AdvertisingData.build do |a|
          a.add(BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS)
          a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, name)
          a.add(BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
                @service_uuid_bin)
        end
        @adv_data = adv_data

        db = GattDatabase.new do |d|
          d.add_service(GATT_PRIMARY_SERVICE_UUID, GAP_SERVICE_UUID) do |s|
            s.add_characteristic(READ, GAP_DEVICE_NAME_UUID, READ, name)
          end
          d.add_service(GATT_PRIMARY_SERVICE_UUID, @service_uuid_bin) do |s|
            # RX: Central writes to this characteristic
            s.add_characteristic(
              WRITE_WITHOUT_RESPONSE | DYNAMIC,
              @rx_uuid_bin,
              WRITE_WITHOUT_RESPONSE | DYNAMIC,
              ""
            )
            # TX: Peripheral notifies via this characteristic
            s.add_characteristic(
              NOTIFY | DYNAMIC,
              @tx_uuid_bin,
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

        @rx_handle = db.handle_table[@service_uuid_bin][@rx_uuid_bin][:value_handle]
        @tx_handle = db.handle_table[@service_uuid_bin][@tx_uuid_bin][:value_handle]
        @cccd_handle = db.handle_table[@service_uuid_bin][@tx_uuid_bin][CLIENT_CHARACTERISTIC_CONFIGURATION]
        @advertising_started = false
        @notification_enabled = false
        super(:peripheral, db.profile_data)
      else
        @uart_central_state = :TC_OFF
        @conn_handle = HCI_CON_HANDLE_INVALID
        @peer_rx_handle = nil
        @peer_tx_handle = nil
        @peer_cccd_handle = nil
        @nus_start_handle = nil
        @nus_end_handle = nil
        super(:central)
      end
    end

    def start(&block)
      @user_block = block
      super()
    end

    def peripheral?
      @role == :peripheral
    end

    def central?
      @role == :central
    end

    def heartbeat_callback
      blink_led
      if peripheral?
        _start_advertise unless @advertising_started
        _drain_rx
        _check_cccd
        _request_send
      else
        _flush_tx_central if @connected
      end
      @user_block&.call
    end

    def packet_callback(event_packet)
      if peripheral?
        _peripheral_packet_callback(event_packet)
      else
        _central_packet_callback(event_packet)
      end
    end

    # --- IO interface (shared between peripheral and central) ---

    def write(data)
      str = data.to_s
      @tx_buffer << str
      if peripheral?
        _request_send
      else
        _flush_tx_central if @connected
      end
      str.bytesize
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

    # --- Peripheral private methods ---

    def _peripheral_packet_callback(event_packet)
      case event_packet[0]&.ord
      when BTSTACK_EVENT_STATE
        return unless event_packet[2]&.ord == HCI_STATE_WORKING
        debug_puts "UART Peripheral up on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        _start_advertise
      when HCI_EVENT_DISCONNECTION_COMPLETE
        @connected = false
        @notification_enabled = false
        @advertising_started = false
        debug_puts "Disconnected, re-advertising"
        _start_advertise
      when ATT_EVENT_MTU_EXCHANGE_COMPLETE
        @connected = true
        debug_puts "Connected (MTU exchange complete)"
      when ATT_EVENT_CAN_SEND_NOW
        _flush_tx
      end
    end

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
      unless chunk.empty?
        @tx_buffer = @tx_buffer.byteslice(NOTIFY_MTU..-1) || ""
        push_read_value(@tx_handle, chunk)
        notify(@tx_handle)
        _request_send
      end
    end

    def _start_advertise
      return if @advertising_started
      advertise(@adv_data)
      @advertising_started = true
      debug_puts "Advertising started"
    end

    # --- Central private methods ---

    def _central_packet_callback(event_packet)
      event_type = event_packet.getbyte(0)
      return unless event_type
      case event_type
      when BTSTACK_EVENT_STATE
        return unless event_packet.getbyte(2) == HCI_STATE_WORKING
        debug_puts "UART Central up on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        set_scan_params(:passive, 0x60, 0x30)
        start_scan
        @uart_central_state = :TC_W4_SCAN_RESULT

      when HCI_EVENT_DISCONNECTION_COMPLETE
        debug_puts "Disconnected, re-scanning"
        _central_reset
        start_scan
        @uart_central_state = :TC_W4_SCAN_RESULT

      when GAP_EVENT_ADVERTISING_REPORT
        return unless @uart_central_state == :TC_W4_SCAN_RESULT
        adv_report = AdvertisingReport.new(event_packet)
        service_data = adv_report.reports[:complete_list_128_bit_service_class_uuids] ||
                       adv_report.reports[:incomplete_list_128_bit_service_class_uuids]
        if service_data&.include?(@service_uuid_bin)
          debug_puts "Found NUS device: #{Utils.bd_addr_to_str(adv_report.address)}"
          stop_scan
          err = gap_connect(adv_report.address, adv_report.address_type_code)
          @uart_central_state = :TC_W4_CONNECT if err == 0
        end

      when HCI_EVENT_LE_META
        return unless event_packet.getbyte(2) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE
        return unless @uart_central_state == :TC_W4_CONNECT
        @conn_handle = Utils.little_endian_to_int16(event_packet.byteslice(4, 2))
        debug_puts "Connected. Handle: #{sprintf('0x%04X', @conn_handle)}"
        discover_primary_services(@conn_handle)
        @uart_central_state = :TC_W4_SERVICE_RESULT

      when GATT_EVENT_QUERY_COMPLETE..GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT
        _central_handle_gatt_event(event_type, event_packet)

      when GATT_EVENT_NOTIFICATION
        value_handle = Utils.little_endian_to_int16(event_packet.byteslice(4, 2))
        return unless value_handle == @peer_tx_handle
        value_length = Utils.little_endian_to_int16(event_packet.byteslice(6, 2))
        value = event_packet.byteslice(8, value_length)
        @rx_buffer << value if value
      end
    end

    def _central_handle_gatt_event(event_type, event_packet)
      case @uart_central_state
      when :TC_W4_SERVICE_RESULT
        case event_type
        when GATT_EVENT_SERVICE_QUERY_RESULT
          # UUID in packet is little-endian, same as @service_uuid_bin from Utils.uuid()
          if event_packet.byteslice(8, 16) == @service_uuid_bin
            @nus_start_handle = Utils.little_endian_to_int16(event_packet.byteslice(4, 2))
            @nus_end_handle   = Utils.little_endian_to_int16(event_packet.byteslice(6, 2))
            debug_puts "NUS service found. Handles: #{@nus_start_handle}..#{@nus_end_handle}"
          end
        when GATT_EVENT_QUERY_COMPLETE
          if (start_h = @nus_start_handle) && (end_h = @nus_end_handle)
            discover_characteristics_for_service(@conn_handle, start_h, end_h)
            @uart_central_state = :TC_W4_CHAR_RESULT
          else
            debug_puts "NUS service not found, re-scanning"
            _central_reset
            start_scan
            @uart_central_state = :TC_W4_SCAN_RESULT
          end
        end

      when :TC_W4_CHAR_RESULT
        case event_type
        when GATT_EVENT_CHARACTERISTIC_QUERY_RESULT
          value_handle = Utils.little_endian_to_int16(event_packet.byteslice(6, 2))
          # UUID at offset 12 is little-endian, same as @rx_uuid_bin / @tx_uuid_bin
          uuid_bin = event_packet.byteslice(12, 16)
          if uuid_bin == @rx_uuid_bin
            @peer_rx_handle = value_handle
            debug_puts "RX handle: #{@peer_rx_handle}"
          elsif uuid_bin == @tx_uuid_bin
            @peer_tx_handle  = value_handle
            # CCCD is always the next handle after the TX value handle in NUS
            @peer_cccd_handle = value_handle + 1
            debug_puts "TX handle: #{@peer_tx_handle}, CCCD: #{@peer_cccd_handle}"
          end
        when GATT_EVENT_QUERY_COMPLETE
          if @peer_rx_handle && @peer_tx_handle && (cccd = @peer_cccd_handle)
            write_characteristic_descriptor_using_descriptor_handle(
              @conn_handle, cccd, "\x01\x00"
            )
            @uart_central_state = :TC_W4_CCCD_WRITE
          else
            debug_puts "NUS characteristics not found, re-scanning"
            _central_reset
            start_scan
            @uart_central_state = :TC_W4_SCAN_RESULT
          end
        end

      when :TC_W4_CCCD_WRITE
        if event_type == GATT_EVENT_QUERY_COMPLETE
          @connected = true
          @uart_central_state = :TC_READY
          debug_puts "NUS central ready"
        end
      end
    end

    def _central_reset
      @connected = false
      @conn_handle = HCI_CON_HANDLE_INVALID
      @peer_rx_handle = nil
      @peer_tx_handle = nil
      @peer_cccd_handle = nil
      @nus_start_handle = nil
      @nus_end_handle = nil
    end

    def _flush_tx_central
      rx_handle = @peer_rx_handle
      return unless rx_handle
      while !@tx_buffer.empty?
        chunk = @tx_buffer.byteslice(0, NOTIFY_MTU) || ""
        break if chunk.empty?
        @tx_buffer = @tx_buffer.byteslice(NOTIFY_MTU..-1) || ""
        write_value_of_characteristic_without_response(@conn_handle, rx_handle, chunk)
      end
    end
  end
end
