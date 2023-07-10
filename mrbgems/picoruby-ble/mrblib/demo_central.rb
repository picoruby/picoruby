class DemoCentral < BLE::Central
  HCI_SUBEVENT_LE_CONNECTION_COMPLETE = 0x01
  HCI_EVENT_DISCONNECTION_COMPLETE = 0x05
  HCI_CON_HANDLE_INVALID = 0xffff
  BTSTACK_EVENT_STATE = 0x60
  HCI_EVENT_LE_META = 0x3E
  GAP_EVENT_ADVERTISING_REPORT = 0xda

  GATT_EVENT_QUERY_COMPLETE = 0xA0
  GATT_EVENT_SERVICE_QUERY_RESULT = 0xA1
  GATT_EVENT_CHARACTERISTIC_QUERY_RESULT = 0xA2

  def initialize
    super(nil)
    @led = CYW43::GPIO.new(CYW43::GPIO::LED_PIN)
    @led_on = false
    # @state can be:
    #   :TC_OFF
    #   :TC_IDLE
    #   :TC_W4_SCAN_RESULT
    #   :TC_W4_CONNECT
    #   :TC_W4_SERVICE_RESULT
    #   :TC_W4_CHARACTERISTIC_RESULT
    #   :TC_W4_ENABLE_NOTIFICATIONS_COMPLETE
    #   :TC_W4_READY
    @found_devices_count_limit = 10
    @conn_handle = HCI_CON_HANDLE_INVALID
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
  end

  def packet_callback(event_packet)
    case event_packet[0]&.ord # event type
    when BTSTACK_EVENT_STATE
      return if @state != :TC_OFF && @state != :TC_IDLE
      if event_packet[2]&.ord == BLE::HCI_STATE_WORKING
        debug_puts "Central is up and running on: `#{BLE::Utils.bd_addr_to_str(gap_local_bd_addr)}`"
        start_scan
        @state = :TC_W4_SCAN_RESULT
      else
        @state = :TC_OFF
      end
    when GAP_EVENT_ADVERTISING_REPORT
      return unless @state == :TC_W4_SCAN_RESULT
      adv_report = BLE::AdvertisingReport.new(event_packet)
      if @found_devices_count_limit <= found_devices.count
        return
      end
      unless found_devices.any?{ |d| d.address == adv_report.address }
        add_found_device adv_report
      end
    when HCI_EVENT_LE_META
      return unless @state == :TC_W4_CONNECT
      debug_puts "event_packet: #{event_packet.inspect}"
      case event_packet[2]&.ord
      when HCI_SUBEVENT_LE_CONNECTION_COMPLETE
        return unless @state == :TC_W4_CONNECT
        @conn_handle = BLE::Utils.little_endian_to_int16(event_packet[4, 2])
        debug_puts "Connected. Handle: `#{sprintf("0x%04X", @conn_handle)}`"
        @state = :TC_W4_SERVICE_RESULT
        err_code = discover_primary_services(@conn_handle)
        if err_code != 0
          puts "Discover primary services failed. Error code: `#{err_code}`"
          @state = :TC_IDLE
        end
      when HCI_EVENT_DISCONNECTION_COMPLETE
        @conn_handle = HCI_CON_HANDLE_INVALID
      end
    when GATT_EVENT_SERVICE_QUERY_RESULT
      return unless @state == :TC_W4_SERVICE_RESULT
      debug_puts "GATT_EVENT_SERVICE_QUERY_RESULT"
      debug_puts "event_packet: #{event_packet.inspect}"
      uuid128 = ""
      15.downto(0) { |i| uuid128 << event_packet[8 + i] }
      @services << {
        start_group_handle: BLE::Utils.little_endian_to_int16(event_packet[4]),
        end_group_handle: BLE::Utils.little_endian_to_int16(event_packet[6]),
        uuid128: uuid128,
        uuid16: ((uuid128[2] || 0).ord << 8) | (uuid128[3] || 0).ord,
        characteristics: []
      }
    when GATT_EVENT_CHARACTERISTIC_QUERY_RESULT
      return unless @state == :TC_W4_CHARACTERISTIC_RESULT
      debug_puts "GATT_EVENT_CHARACTERISTIC_QUERY_RESULT"
      debug_puts "event_packet: #{event_packet.inspect}"
      uuid128 = ""
      15.downto(0) { |i| uuid128 << event_packet[12 + i] }
      @services[@_discovering_characteristics_index][:characteristics] << {
        start_handle: BLE::Utils.little_endian_to_int16(event_packet[4]),
        value_handle: BLE::Utils.little_endian_to_int16(event_packet[6]),
        end_handle: BLE::Utils.little_endian_to_int16(event_packet[8]),
        properties: BLE::Utils.little_endian_to_int16(event_packet[10]),
        uuid128: uuid128,
        uuid16: ((uuid128[2] || 0).ord << 8) | (uuid128[3] || 0).ord
      }
      if @_discovering_characteristics_index < @services.count - 1
        debug_puts "discovering next"
        @_discovering_characteristics_index += 1
        @services[@_discovering_characteristics_index][:characteristics].clear
        discover_characteristics_for_service(@conn_handle, @services[@_discovering_characteristics_index])
      else
        @state = :TC_W4_READY
      end
    when GATT_EVENT_QUERY_COMPLETE
      case @state
      when :TC_W4_SERVICE_RESULT
        debug_puts "GATT_EVENT_QUERY_COMPLETE for service"
        @services[0][:characteristics].clear
        discover_characteristics_for_service(@conn_handle, @services[0])
        @_discovering_characteristics_index = 0
        @state = :TC_W4_CHARACTERISTIC_RESULT
      when :TC_W4_CHARACTERISTIC_RESULT
        debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic"
        @state = :TC_W4_READY
      end
    end
  end
end
