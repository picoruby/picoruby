class BLE
  class Utils
    def self.bd_addr_to_str(addr)
      addr.bytes.map{|b| sprintf("%02X", b)}.join(":")
    end

    def self.uuid(value)
      case value
      when Integer
        str = int16_to_little_endian(value)
      when String
        # Simply trustily assumes "cd833ba3-97c5-4615-a2a0-a6c3e56b24b2" format
        str = "_" * 16
        i = 0
        15.downto(0) do |j|
          i += 1 if value[i] == "-"
          c = value[i, 2].to_s
          if c.length != 2
            raise ArgumentError, "invalid uuid value: `#{value}`"
          end
          if valid_char_for_uuid?(c[0]) && valid_char_for_uuid?(c[1])
            str[j] = c.to_i(16).chr
          else
            str = ""
            break 0
          end
          i += 2
        end
      else
        str = ""
      end
      if str.empty?
        raise ArgumentError, "invalid uuid value: `#{value}`"
      end
      str
    end

    def self.reverse_128(src)
      dst = ""
      15.downto(0) { |i| dst << src[i] }
    end

    # Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB
    def self.uuid128_to_uuid32(uuid128)
      if uuid128.length == 16 && uuid128[4, 12] == "\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB"
        (uuid128[0].ord | (uuid128[1].ord << 8) | (uuid128[2].ord << 16) | (uuid128[3].ord << 24))
      else
        nil
      end
    end

    def self.int16_to_little_endian(value)
      (value & 0xff).chr + (value >> 8 & 0xff).chr
    end

    def self.int32_to_little_endian(value)
      int16_to_little_endian(value & 0xffff) + int16_to_little_endian(value >> 16 & 0xffff)
    end

    def self.little_endian_to_int16(str)
      if 2 < str.length
        raise ArgumentError, "invalid length of string: #{str.length}"
      end
      (str[0]&.ord || 0) | ((str[1]&.ord || 0) << 8)
    end

    def self.little_endian_to_int32(str)
      if 4 < str.length
        raise ArgumentError, "invalid length of string: #{str.length}"
      end
      little_endian_to_int16(str[0, 2] || "") | (little_endian_to_int16(str[2, 2] || "") << 16)
    end

    # private

    def self.valid_char_for_uuid?(c)
      o = c&.downcase
      ('0'..'9') === o || ('a'..'f') === o
    end

  end
end
