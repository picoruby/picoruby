class BLE
  class AdvertisingReport
    EVENT_TYPE = {
      0x00 => :connectable_advertising_ind,
      0x01 => :flags,
      0x02 => :incomplete_list_16_bit_service_class_uuids,
      0x03 => :complete_list_16_bit_service_class_uuids,
      0x04 => :incomplete_list_32_bit_service_class_uuids,
      0x05 => :complete_list_32_bit_service_class_uuids,
      0x06 => :incomplete_list_128_bit_service_class_uuids,
      0x07 => :complete_list_128_bit_service_class_uuids,
      0x08 => :shortened_local_name,
      0x09 => :complete_local_name,
      0x0A => :tx_power_level,
      0xff => :manufacturer_specific_data
    }
    ADDRESS_TYPE = [
      :public,
      :random,
      :public_identity,
      :random_identity
    ]
    attr_reader :event_type, :address_type_code, :address, :rssi, :reports

    def initialize(packet)
      if packet.length < 14
        raise ArgumentError, "packet length must be 14 or more"
      end
      event_code = packet[2]&.ord
      @event_type = EVENT_TYPE[event_code || -1] || sprintf("0x%02x", event_code)
      @address_type_code = packet[3]&.ord || 99
      @address = ""
      5.downto(0) do |i|
        @address << (packet[4 + i] || "\x00")
      end
      @rssi = (packet[10]&.ord || 0) - 256
      data_length = packet[11]&.ord || 0
      @reports = inspect_reports(packet[12, data_length] || "")
    end

    def format
      "Event Type: #{@event_type}" +
      "\nAddress Type: #{ADDRESS_TYPE[@address_type_code] || 'unknown'}" +
      "\nAddress: #{BLE::Utils.bd_addr_to_str(@address)}" +
      "\nRSSI: #{@rssi}" +
      "\nReports:\n" +
      @reports.map { |type, value|
        "  #{type}: #{value.inspect}(len #{value.length})"
      }.join("\n")
    end

    def name_include?(name)
      @reports[:shortened_local_name]&.include?(name) || @reports[:complete_local_name]&.include?(name)
    end

    # private

    def inspect_reports(data)
      reports = {}
      index = 0
      while index < data.length
        length = data[index]&.ord
        break if length.nil?
        type_num = data[index + 1]&.ord
        break if type_num.nil?
        value = data[index + 2, length - 1] || ""
        break if value.empty?
        reports[EVENT_TYPE[type_num] || type_num] = value
        index += length + 1
      end
      reports
    end

  end
end
