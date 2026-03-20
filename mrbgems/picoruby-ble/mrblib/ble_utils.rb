require 'pack'

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
            str[j] = [c.to_i(16)].pack("C")
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
      if src.nil? || src.bytesize != 16
        raise ArgumentError, "invalid length of string: #{src.inspect}"
      end
      dst = ""
      15.downto(0) { |i| dst << [src.getbyte(i) || 0].pack('C') }
      dst
    end

    # Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB
    def self.uuid128_to_uuid32(uuid128)
      if uuid128.bytesize == 16 && uuid128.byteslice(4, 12) == "\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB"
        ((uuid128.getbyte(0) || 0) | ((uuid128.getbyte(1) || 0) << 8) | ((uuid128.getbyte(2) || 0) << 16) | ((uuid128.getbyte(3) || 0) << 24))
      else
        nil
      end
    end

    def self.int16_to_little_endian(value)
      [value & 0xff, value >> 8 & 0xff].pack("CC")
    end

    def self.int32_to_little_endian(value)
      int16_to_little_endian(value & 0xffff) + int16_to_little_endian(value >> 16 & 0xffff)
    end

    def self.little_endian_to_int16(str)
      if str.nil? || 2 < str.bytesize
        raise ArgumentError, "invalid length of string: #{str.inspect}"
      end
      (str.getbyte(0) || 0) | ((str.getbyte(1) || 0) << 8)
    end

    def self.little_endian_to_int32(str)
      if str.nil? || 4 < str.bytesize
        raise ArgumentError, "invalid length of string: #{str.inspect}"
      end
      little_endian_to_int16(str.byteslice(0, 2)) | (little_endian_to_int16(str.byteslice(2, 2)) << 16)
    end

    # private

    def self.valid_char_for_uuid?(c)
      o = c&.downcase
      ('0'..'9') === o || ('a'..'f') === o
    end

  end
end
