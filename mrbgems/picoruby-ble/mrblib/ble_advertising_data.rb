class BLE
  class AdvertisingData
    def initialize
      @data = ""
    end

    attr_reader :data

    def add(type, *values)
      # @data is binary, so count bytes and never index it by character
      length_pos = @data.bytesize
      @data << [0].pack("C") # dummy length
      @data << [type].pack("C")
      length = 1
      vi = 0
      while vi < values.size
        d = values[vi]
        case d
        when String
          @data << d
          length += d.bytesize
        when 0
          @data << "\x00"
          length += 1
        when Integer
          while 0 < d
            @data << [d & 0xff].pack("C")
            d >>= 8
            length += 1
          end
        when nil
          @data << "\x00"
          length += 1
        else
          raise ArgumentError, "invalid data type: `#{d}`"
        end
        vi += 1
      end
      @data.setbyte(length_pos, length)
    end

    def self.build(&block)
      instance = self.new
      block.call(instance)
      adv_data = instance.data
      if 31 < adv_data.bytesize
        raise ArgumentError, "too long AdvData: (#{adv_data.bytesize} bytes). It must be less than 32 bytes."
      end
      adv_data
    end
  end
end
