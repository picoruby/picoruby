class BLE
  class Central < BLE

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

    def initialize(profile_data = nil)
      super(profile_data)
      @found_devices = []
      reset_state
      @services = []
      # @state can be:
      #   :TC_OFF
      #   :TC_IDLE
      #   :TC_W4_SCAN_RESULT
      #   :TC_W4_CONNECT
      #   :TC_W4_SERVICE_RESULT
      #   :TC_W4_CHARACTERISTIC_RESULT
      #   :TC_W4_CHARACTERISTIC_VALUE_RESULT
      #   :TC_W4_ALL_CHARACTERISTIC_DESCRIPTORS_RESULT
      #   :TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT
      #   :TC_W4_ENABLE_NOTIFICATIONS_COMPLETE
      #   :TC_W4_READY
      @found_devices_count_limit = 10
      @conn_handle = HCI_CON_HANDLE_INVALID
      @characteristic_handle_ranges = []
      @value_handles = []
      @descriptor_handle_ranges = []
      @descriptor_handles = []
    end

    attr_reader :found_devices, :services, :state

    def scan(search_name = nil)
      @found_devices_count_limit = 1 if search_name
      @search_name = search_name
      start(10, :TC_IDLE)
      0 < @found_devices.size
    end

    def reset_state
      @state = :TC_OFF
    end

    def connect(device_id)
      unless device = @found_devices[device_id]
        puts "No device with id #{device_id} found."
        return false
      end
      stop_scan # Is it necessary?
      @_event_packets.clear
      err_code = gap_connect(device.address, device.address_type)
      if err_code == 0
        @state = :TC_W4_CONNECT
        start(10, :TC_IDLE)
        return true
      else
        puts "Error: #{err_code}"
        return false
      end
    end

    def add_found_device(adv_report)
      @found_devices << adv_report
    end

    def clear_found_devices
      @found_devices = []
    end

    def print_found_devices
      @found_devices.each_with_index do |adv_report, index|
        puts "ID: #{index}"
        puts adv_report.format
        puts ""
      end
      nil
    end

    def packet_callback(event_packet)
      event_type = event_packet[0]&.ord
      case event_type
      when BTSTACK_EVENT_STATE
        return if @state != :TC_OFF && @state != :TC_IDLE
        if event_packet[2]&.ord == HCI_STATE_WORKING
          debug_puts "Central is up and running on: `#{Utils.bd_addr_to_str(gap_local_bd_addr)}`"
          start_scan
          @state = :TC_W4_SCAN_RESULT
        else
          @state = :TC_OFF
        end
      when GAP_EVENT_ADVERTISING_REPORT
        return unless @state == :TC_W4_SCAN_RESULT
        adv_report = AdvertisingReport.new(event_packet)
        if @found_devices_count_limit <= found_devices.count
          @state = :TC_IDLE
          return
        end
        unless found_devices.any?{ |d| d.address == adv_report.address }
          if @search_name.nil? || adv_report.name_include?(@search_name)
            add_found_device adv_report
          end
        end
      when HCI_EVENT_LE_META
        return unless @state == :TC_W4_CONNECT
        case event_packet[2]&.ord
        when HCI_SUBEVENT_LE_CONNECTION_COMPLETE
          @conn_handle = Utils.little_endian_to_int16(event_packet[4, 2])
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
            start_handle = Utils.little_endian_to_int16(event_packet[4])
            end_handle = Utils.little_endian_to_int16(event_packet[6])
            uuid128 = Utils.reverse_128(event_packet[8, 16])
            @services << {
              start_handle: start_handle,
              end_handle: end_handle,
              uuid128: uuid128,
              uuid32: Utils.uuid128_to_uuid32(uuid128),
              characteristics: []
            }
            @characteristic_handle_ranges << { start_handle: start_handle, end_handle: end_handle }
          when GATT_EVENT_QUERY_COMPLETE
            debug_puts "GATT_EVENT_QUERY_COMPLETE for service"
            if characteristic_handle_range = @characteristic_handle_ranges.shift
              discover_characteristics_for_service(
                @conn_handle,
                characteristic_handle_range[:start_handle],
                characteristic_handle_range[:end_handle]
              )
              @state = :TC_W4_CHARACTERISTIC_RESULT
            else
              @state = :TC_IDLE
            end
          end
        when :TC_W4_CHARACTERISTIC_RESULT
          case event_type
          when GATT_EVENT_CHARACTERISTIC_QUERY_RESULT
            debug_puts "GATT_EVENT_CHARACTERISTIC_QUERY_RESULT"
            start_handle = Utils.little_endian_to_int16(event_packet[4])
            value_handle = Utils.little_endian_to_int16(event_packet[6])
            end_handle = Utils.little_endian_to_int16(event_packet[8])
            uuid128 = Utils.reverse_128(event_packet[12, 16])
            # @type var characteristic: characteristic_t
            characteristic = {
              start_handle: start_handle,
              value_handle: value_handle,
              end_handle: end_handle,
              properties: Utils.little_endian_to_int16(event_packet[10]),
              uuid128: uuid128,
              uuid32: Utils.uuid128_to_uuid32(uuid128),
              value: nil,
              descriptors: []
            }
            @services.each do |service|
              if service[:start_handle] < start_handle && end_handle <= service[:end_handle]
                service[:characteristics] << characteristic
                break []
              end
            end
            @value_handles << value_handle
            if value_handle < end_handle
              @descriptor_handle_ranges << { value_handle: value_handle, end_handle: end_handle }
            end
          when GATT_EVENT_QUERY_COMPLETE
            debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic"
            if characteristic_handle_range = @characteristic_handle_ranges.shift
              discover_characteristics_for_service(
                @conn_handle,
                characteristic_handle_range[:start_handle],
                characteristic_handle_range[:end_handle]
              )
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
            @services.each do |service|
              service[:characteristics].each do |chara|
                if chara[:value_handle] == Utils.little_endian_to_int16(event_packet[4])
                  chara[:value] = event_packet[8, Utils.little_endian_to_int16(event_packet[6])]
                  break []
                end
              end
            end
            if value_handle = @value_handles.shift
              @_event_packets.clear
              read_value_of_characteristic_using_value_handle(@conn_handle, value_handle)
            end
          when GATT_EVENT_QUERY_COMPLETE
            debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic value"
            if handle_range = @descriptor_handle_ranges.shift
              @_event_packets.clear
              discover_characteristic_descriptors(@conn_handle, handle_range[:value_handle], handle_range[:end_handle])
              @state = :TC_W4_ALL_CHARACTERISTIC_DESCRIPTORS_RESULT
            else
              @state = :TC_IDLE
            end
          end
        when :TC_W4_ALL_CHARACTERISTIC_DESCRIPTORS_RESULT
          case event_type
          when GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT
            debug_puts "GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT"
            handle = Utils.little_endian_to_int16(event_packet[4])
            uuid128 = Utils.reverse_128(event_packet[6, 16])
            @services.each do |service|
              service[:characteristics].each do |chara|
                if chara[:value_handle] < handle && handle <= chara[:end_handle]
                  chara[:descriptors] << {
                    handle: handle,
                    uuid128: uuid128,
                    uuid32: Utils.uuid128_to_uuid32(uuid128),
                    value: nil
                  }
                end
              end
            end
            @descriptor_handles << handle
          when GATT_EVENT_QUERY_COMPLETE
            debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic descriptor"
            if handle_range = @descriptor_handle_ranges.shift
              @_event_packets.clear
              discover_characteristic_descriptors(@conn_handle, handle_range[:value_handle], handle_range[:end_handle])
            elsif descriptor_handle = @descriptor_handles.shift
              @_event_packets.clear
              # I don't know why, but read_value_of_characteristic_descriptor() doesn't work.
              read_value_of_characteristic_using_value_handle(@conn_handle, descriptor_handle)
              @state = :TC_W4_CHARACTERISTIC_DESCRIPTOR_VALUE_RESULT
            else
              @state = :TC_IDLE
            end
          end
        when :TC_W4_CHARACTERISTIC_DESCRIPTOR_VALUE_RESULT
          case event_type
          when GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            debug_puts "GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT"
            @services.each do |service|
              service[:characteristics].each do |chara|
                chara[:descriptors].each do |descriptor|
                  if descriptor[:handle] == Utils.little_endian_to_int16(event_packet[4])
                    descriptor[:value] = event_packet[8, Utils.little_endian_to_int16(event_packet[6])]
                    break []
                  end
                end
              end
            end
            if descriptor_handle = @descriptor_handles.shift
              @_event_packets.clear
              # I don't know why, but read_value_of_characteristic_descriptor() doesn't work.
              read_value_of_characteristic_using_value_handle(@conn_handle, descriptor_handle)
            end
          when GATT_EVENT_QUERY_COMPLETE
            debug_puts "GATT_EVENT_QUERY_COMPLETE for characteristic descriptor value"
            @state = :TC_IDLE
          end
        else
          debug_puts "Not implemented: 0x#{event_type&.to_s(16)} state: #{@state}"
        end
      when GATT_EVENT_NOTIFICATION
        #when :TC_W4_ENABLE_NOTIFICATIONS_COMPLETE
        # TODO
      end
    end

  end
end
