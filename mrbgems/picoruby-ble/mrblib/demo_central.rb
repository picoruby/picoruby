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
  GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT = 0xA3
  GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT = 0xA4
  GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT = 0xA5
  GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT = 0xA6
  GATT_EVENT_NOTIFICATION = 0xA7
  GATT_EVENT_INDICATION = 0xA8
  GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT = 0xA9
  GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT = 0xAA

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
    #   :TC_W4_CHARACTERISTIC_VALUE_RESULT
    #   :TC_W4_CHARACTERISTIC_DESCRIPTORS_RESULT
    #   :TC_W4_ENABLE_NOTIFICATIONS_COMPLETE
    #   :TC_W4_READY
    @found_devices_count_limit = 10
    @conn_handle = HCI_CON_HANDLE_INVALID
    @value_handles = []
    @descriptor_handles = []
  end

  def heartbeat_callback
    @led.write((@led_on = !@led_on) ? 1 : 0)
  end

  def packet_callback(event_packet)
    event_type = event_packet[0]&.ord
    case event_type
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
        @state = :TC_IDLE
        return
      end
      unless found_devices.any?{ |d| d.address == adv_report.address }
        if !@search_name || adv_report.name_include?(@search_name)
          add_found_device adv_report
        end
      end
    when HCI_EVENT_LE_META
      return unless @state == :TC_W4_CONNECT
      #debug_puts "event_packet: #{event_packet.inspect}"
      case event_packet[2]&.ord
      when HCI_SUBEVENT_LE_CONNECTION_COMPLETE
        @conn_handle = BLE::Utils.little_endian_to_int16(event_packet[4, 2])
        debug_puts "Connected. Handle: `#{sprintf("0x%04X", @conn_handle)}`"
        @state = :TC_W4_SERVICE_RESULT
        @services.clear
        err_code = discover_primary_services(@conn_handle)
        if err_code != 0
          puts "Discover primary services failed. Error code: `#{err_code}`"
          @state = :TC_IDLE
        end
      when HCI_EVENT_DISCONNECTION_COMPLETE
        @conn_handle = HCI_CON_HANDLE_INVALID
      end
    when GATT_EVENT_QUERY_COMPLETE..GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT
      # Build @services
      case @state
      when :TC_W4_SERVICE_RESULT
        case event_type
        when GATT_EVENT_SERVICE_QUERY_RESULT
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
        when GATT_EVENT_QUERY_COMPLETE
          debug_puts "GATT_EVENT_QUERY_COMPLETE for service"
          @service_index = 0
          discover_characteristics_for_service(@conn_handle, @services[@service_index])
          @state = :TC_W4_CHARACTERISTIC_RESULT
        end
      when :TC_W4_CHARACTERISTIC_RESULT
        case event_type
        when GATT_EVENT_CHARACTERISTIC_QUERY_RESULT
          debug_puts "GATT_EVENT_CHARACTERISTIC_QUERY_RESULT"
          debug_puts "event_packet: #{event_packet.inspect}"
          uuid128 = ""
          value_handle = BLE::Utils.little_endian_to_int16(event_packet[6])
          end_handle = BLE::Utils.little_endian_to_int16(event_packet[8])
          15.downto(0) { |i| uuid128 << event_packet[12 + i] }
          characteristic = {
            start_handle: BLE::Utils.little_endian_to_int16(event_packet[4]),
            value_handle: value_handle,
            end_handle: end_handle,
            properties: BLE::Utils.little_endian_to_int16(event_packet[10]),
            uuid128: uuid128,
            uuid16: ((uuid128[2] || 0).ord << 8) | (uuid128[3] || 0).ord,
            value: nil,
            descriptors: []
          }
          @services[@service_index][:characteristics] << characteristic
          @value_handles << value_handle
          (value_handle + 1).upto(end_handle) do |handle|
            @descriptor_handles << handle
          end
        when GATT_EVENT_QUERY_COMPLETE
          debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic"
          @service_index += 1
          if @service_index < @services.size
            discover_characteristics_for_service(@conn_handle, @services[@service_index])
          elsif value_handle = @value_handles.shift
            @_event_packets.clear
            read_value_of_characteristic_using_value_handle(@conn_handle, value_handle)
            @state = :TC_W4_CHARACTERISTIC_VALUE_RESULT
          end
        end
      when :TC_W4_CHARACTERISTIC_VALUE_RESULT
        case event_type
        when GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
          debug_puts "GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT"
          debug_puts "event_packet: #{event_packet.inspect}"
          value_handle = BLE::Utils.little_endian_to_int16(event_packet[4])
          length = BLE::Utils.little_endian_to_int16(event_packet[6])
          @services.each do |service|
            service[:characteristics].each do |chara|
              if chara[:value_handle] == value_handle
                chara[:value] = event_packet[8, length]
                break
              end
            end
          end
          if value_handle = @value_handles.shift
            @_event_packets.clear
            read_value_of_characteristic_using_value_handle(@conn_handle, value_handle)
          end
        when GATT_EVENT_QUERY_COMPLETE
          debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic value"
          debug_puts "event_packet: #{event_packet.inspect}"
          if descriptor_handle = @descriptor_handles.shift
            debug_puts "descriptor_handle: #{descriptor_handle}"
            @_event_packets.clear
            read_characteristic_descriptor_using_descriptor_handle(@conn_handle, descriptor_handle)
            @state = :TC_W4_CHARACTERISTIC_DESCRIPTORS_RESULT
          else
            @state = :TC_IDLE
          end
        end
      when :TC_W4_CHARACTERISTIC_DESCRIPTORS_RESULT
        case event_type
        when GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT
          debug_puts "GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT"
          debug_puts "event_packet: #{event_packet.inspect}"
          handle = BLE::Utils.little_endian_to_int16(event_packet[2])
          uuid128 = ""
          15.downto(0) { |i| uuid128 << event_packet[6 + i] }
          @services.each do |service|
            service[:characteristics].each do |chara|
              if chara[:value_handle] < handle && handle <= chara[:end_handle]
                chara[:descriptors] << {
                  handle: handle,
                  uuid128: uuid128,
                  uuid16: ((uuid128[2] || 0).ord << 8) | (uuid128[3] || 0).ord,
                  value: nil
                }
              end
            end
          end
          if descriptor_handle = @descriptor_handles.shift
            @_event_packets.clear
            read_characteristic_descriptor_using_descriptor_handle(@conn_handle, descriptor_handle)
          end
        when GATT_EVENT_QUERY_COMPLETE
          debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic descriptor"
          @state = :TC_IDLE
        end
        # TODO
      else
        debug_puts "Not implemented: 0x#{event_type.to_s(16)} state: #{@state}"
      end
    when GATT_EVENT_NOTIFICATION
      #when :TC_W4_ENABLE_NOTIFICATIONS_COMPLETE
       # TODO
    end
  end
end
