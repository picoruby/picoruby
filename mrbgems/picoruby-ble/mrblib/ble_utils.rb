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

    def self.int16_to_little_endian(value)
      (value & 0xff).chr + (value >> 8 & 0xff).chr
    end

    def self.little_endian_to_int16(str)
      str[0]&.ord | (str[1]&.ord << 8)
    end

    # private

    def self.valid_char_for_uuid?(c)
      o = c&.downcase
      ('0'..'9') === o || ('a'..'f') === o
    end

  end
end
