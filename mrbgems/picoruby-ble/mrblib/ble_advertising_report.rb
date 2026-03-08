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
      if packet.bytesize < 14
        raise ArgumentError, "packet length must be 14 or more"
      end
      event_code = packet.getbyte(2)
      @event_type = EVENT_TYPE[event_code || -1] || sprintf("0x%02x", event_code)
      @address_type_code = packet.getbyte(3) || 99
      @address = ""
      5.downto(0) do |i|
        @address << [packet.getbyte(4 + i) || 0].pack('C')
      end
      @rssi = (packet.getbyte(10) || 0) - 256
      data_length = packet.getbyte(11) || 0
      @reports = inspect_reports(packet.byteslice(12, data_length) || raise(ArgumentError, "packet too short"))
    end

    def format
      format_str = "Event Type: #{@event_type}" +
        "\nAddress Type: #{ADDRESS_TYPE[@address_type_code] || 'unknown'}" +
        "\nAddress: #{BLE::Utils.bd_addr_to_str(@address)}" +
        "\nRSSI: #{@rssi}" +
        "\nReports:\n"
      @reports.each do |type, value|
        format_str << "  #{type}: #{value.inspect}(len #{value.length})\n"
      end
      format_str
    end

    def name_include?(name)
      @reports[:shortened_local_name]&.include?(name) || @reports[:complete_local_name]&.include?(name)
    end

    # private

    def inspect_reports(data)
      reports = {} #: Hash[Symbol | Integer, String]
      index = 0
      while index < data.bytesize
        length = data.getbyte(index)
        break if length.nil?
        type_num = data.getbyte(index + 1)
        break if type_num.nil?
        value = data.byteslice(index + 2, length - 1)
        break if value.nil? || value.empty?
        reports[EVENT_TYPE[type_num] || type_num] = value
        index += length + 1
      end
      reports
    end

  end
end
